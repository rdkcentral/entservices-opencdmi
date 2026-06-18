/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "open_cdm_adapter.h"  // OpenCDMError, OpenCDMSession, opencdm_session_status, KeyStatus

#include <gst/gst.h>           // GstBuffer, GstCaps, GstMapInfo, GstProtectionMeta, gst_buffer_map/unmap,
                               //   gst_buffer_get_protection_meta, gst_structure_get_value, gst_value_get_buffer
#include <gst_svp_meta.h>      // RDKPerf (via rdk_perf.h)

#include <chrono>              // std::chrono::milliseconds, seconds, steady_clock
#include <cstdarg>             // va_list, va_start, va_end — adapter_logging
#include <cstdint>             // uint8_t, uint32_t
#include <cstdio>              // FILE*, fopen, fgets, fclose, fprintf, vfprintf, fflush — adapter_logging, readLogLevelFromConfig
#include <cstring>             // strncmp — readLogLevelFromConfig
#include <iomanip>             // std::hex, std::setfill, std::setw — toHexString
#include <sstream>             // std::ostringstream — toHexString
#include <thread>              // std::this_thread::sleep_for
#include <vector>              // std::vector<uint8_t>

OpenCDMError opencdm_gstreamer_session_decrypt_once(struct OpenCDMSession* session, GstBuffer* buffer,
                                                    GstBuffer* subSample, const uint32_t subSampleCount,
                                                    GstBuffer* IV, GstBuffer* keyID, uint32_t initWithLast15);

OpenCDMError opencdm_gstreamer_session_decrypt_buffer_once(struct OpenCDMSession* session, GstBuffer* buffer,
                                                           GstCaps* caps);

namespace
{
#define LOGDECRYPT(level, fmt, ...) adapter_logging(level, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

enum LogLevel {
    ERROR       = 0,
    WARNING     = 1,
    INFO        = 2
};

static LogLevel readLogLevelFromConfig()
{
    static constexpr const char* kConfigPath = "/opt/rdk_adapter_logging.conf";
    FILE* fp = fopen(kConfigPath, "r");
    if (fp == nullptr) {
        return WARNING;
    }
    char line[64];
    LogLevel result = WARNING;
    while (fgets(line, sizeof(line), fp) != nullptr) {
        const char* p = line;
        while (*p == ' ' || *p == '\t') { ++p; }
        // Accept both "level=LEVELNAME" and bare "LEVELNAME"
        const char* val = p;
        if (strncmp(p, "level=", 6) == 0) {
            val = p + 6;
        }
        if (strncmp(val, "ERROR",   5) == 0) { result = ERROR;   break; }
        if (strncmp(val, "WARNING", 7) == 0) { result = WARNING; break; }
        if (strncmp(val, "INFO",    4) == 0) { result = INFO;    break; }
    }
    fclose(fp);
    return result;
}

static LogLevel gLogLevel = readLogLevelFromConfig();

static const char* levelToString(int level)
{
    switch (level) {
        case 0: return "ERROR";
        case 1: return "WARNING";
        case 2: return "INFO";
        default: return "UNKNOWN";
    }
}

static void adapter_logging(int level, const char* func, int line, const char* fmt, ...)
{
    FILE* fp = stdout;
    if (level == ERROR) {
        fp = stderr;
    }
    if(gLogLevel >= level) {
        va_list args;
        va_start(args, fmt);
        fprintf(fp, "%s - %s:%d: ", levelToString(level), func, line);
        vfprintf(fp, fmt, args);
        fprintf(fp, "\n");
        fflush(fp);
        va_end(args);
    }
}

static std::string toHexString(const std::vector<uint8_t>& data)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data) {
        oss << std::setw(2) << static_cast<unsigned>(byte);
    }
    return oss.str();
}

constexpr std::chrono::milliseconds kOutputRestrictedRetryInterval{250};
constexpr std::chrono::seconds kOutputRestrictedRetryTimeout{6};

bool isOutputRestrictedStatus(KeyStatus status)
{
    return (status == OutputRestricted) || (status == OutputRestrictedHDCP22);
}

bool isUsableStatus(KeyStatus status)
{
    return ( (status == Usable) ||
             (status == OutputDownscaled) ||
             (status == StatusPending) );
}

KeyStatus getKeyStatus(struct OpenCDMSession* session, const std::vector<uint8_t>& keyId)
{
    if ((session == nullptr) || keyId.empty() || (keyId.size() > 255)) {
        return InternalError;
    }
    RDKPerf(__FUNCTION__);
    return opencdm_session_status(session, keyId.data(), static_cast<uint8_t>(keyId.size()));
}

bool copyKeyId(GstBuffer* keyID, std::vector<uint8_t>& keyId)
{
    if (keyID == nullptr) {
        return false;
    }

    GstMapInfo keyIDMap;
    if (gst_buffer_map(keyID, &keyIDMap, GST_MAP_READ) == false) {
        return false;
    }

    const uint8_t* mappedKeyID = reinterpret_cast<const uint8_t*>(keyIDMap.data);
    keyId.assign(mappedKeyID, mappedKeyID + keyIDMap.size);
    gst_buffer_unmap(keyID, &keyIDMap);
    return true;
}

bool copyKeyIdFromProtectionMeta(GstBuffer* buffer, std::vector<uint8_t>& keyId)
{
    GstProtectionMeta* protectionMeta = reinterpret_cast<GstProtectionMeta*>(gst_buffer_get_protection_meta(buffer));
    if (protectionMeta == nullptr) {
        return false;
    }

    const GValue* value = gst_structure_get_value(protectionMeta->info, "kid");
    if (value == nullptr) {
        return false;
    }

    return copyKeyId(gst_value_get_buffer(value), keyId);
}

template <typename DecryptOnce>
OpenCDMError decryptWithOutputRestrictedRetry(struct OpenCDMSession* session, const std::vector<uint8_t>& keyId,
                                              DecryptOnce decryptOnce)
{
    if (keyId.empty()) {
        return decryptOnce();
    }

    RDKPerf(__FUNCTION__);
    LOGDECRYPT(INFO, "%s - Starting decryption with output restricted retry logic for key ID: %s",
          __FUNCTION__, toHexString(keyId).c_str());

    const auto deadline = std::chrono::steady_clock::now() + kOutputRestrictedRetryTimeout;
    OpenCDMError lastResult = ERROR_FAIL;

    while (std::chrono::steady_clock::now() < deadline) {
        KeyStatus preDecryptStatus = getKeyStatus(session, keyId);
        if (isOutputRestrictedStatus(preDecryptStatus)) {
            LOGDECRYPT(WARNING, "Key status is output restricted (%d), retrying decryption after delay.", preDecryptStatus);
            std::this_thread::sleep_for(kOutputRestrictedRetryInterval);
            continue;
        }

        if (!isUsableStatus(preDecryptStatus)) {
            LOGDECRYPT(WARNING, "Output protection retry aborted for non-usable key status: %d", preDecryptStatus);
            return ERROR_FAIL;
        }

        lastResult = decryptOnce();
        if (lastResult == ERROR_NONE) {
            return lastResult;
        }

        KeyStatus postDecryptStatus = getKeyStatus(session, keyId);
        if (!isOutputRestrictedStatus(postDecryptStatus)) {
            LOGDECRYPT(WARNING, "Decryption failed with key status (%d), not retrying.", postDecryptStatus);
            return lastResult;
        }

        LOGDECRYPT(WARNING, "Decryption failed with key status (%d), retrying after delay.", postDecryptStatus);
        std::this_thread::sleep_for(kOutputRestrictedRetryInterval);
    }

    LOGDECRYPT(ERROR, "Output protection retry timed out.");
    return lastResult;
}
} // namespace

OpenCDMError opencdm_gstreamer_session_decrypt(struct OpenCDMSession* session, GstBuffer* buffer,
                                               GstBuffer* subSample, const uint32_t subSampleCount,
                                               GstBuffer* IV, GstBuffer* keyID, uint32_t initWithLast15)
{
    RDKPerf(__FUNCTION__);
    std::vector<uint8_t> keyId;
    copyKeyId(keyID, keyId);

    return decryptWithOutputRestrictedRetry(session, keyId, [=]() {
        return opencdm_gstreamer_session_decrypt_once(session, buffer, subSample, subSampleCount, IV, keyID,
                                                      initWithLast15);
    });
}

OpenCDMError opencdm_gstreamer_session_decrypt_buffer(struct OpenCDMSession* session, GstBuffer* buffer, GstCaps* caps)
{
    RDKPerf(__FUNCTION__);
    std::vector<uint8_t> keyId;
    copyKeyIdFromProtectionMeta(buffer, keyId);

    return decryptWithOutputRestrictedRetry(session, keyId, [=]() {
        return opencdm_gstreamer_session_decrypt_buffer_once(session, buffer, caps);
    });
}
