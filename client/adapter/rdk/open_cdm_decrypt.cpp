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

#include "Module.h"
#include "open_cdm_adapter.h"  // OpenCDMError, OpenCDMSession, opencdm_session_status, KeyStatus

#include <gst/gst.h>           // GstBuffer, GstCaps, GstMapInfo, GstProtectionMeta, gst_buffer_map/unmap,
                               //   gst_buffer_get_protection_meta, gst_structure_get_value, gst_value_get_buffer
#include <gst_svp_meta.h>      // RDKPerf (via rdk_perf.h)

#include <atomic>              // std::atomic_flag - one-shot batch capability warning
#include <chrono>              // std::chrono::milliseconds, seconds, steady_clock
#include <cstdarg>             // va_list, va_start, va_end — adapter_logging
#include <cstdint>             // uint8_t, uint32_t
#include <cstdio>              // vsnprintf
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

static void adapter_logging(int level, const char* func, int line, const char* fmt, ...)
{
    char message[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    if (level <= WARNING) {
        TRACE_L1("%s - %s:%d: %s", (level == ERROR) ? "ERROR" : "WARNING", func, line, message);
    } else {
        TRACE_L3("INFO - %s:%d: %s", func, line, message);
    }
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
    RDKPerf perf(__FUNCTION__);
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

    RDKPerf perf(__FUNCTION__);

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
    RDKPerf perf(__FUNCTION__);
    std::vector<uint8_t> keyId;
    copyKeyId(keyID, keyId);

    return decryptWithOutputRestrictedRetry(session, keyId, [=]() {
        return opencdm_gstreamer_session_decrypt_once(session, buffer, subSample, subSampleCount, IV, keyID,
                                                      initWithLast15);
    });
}

OpenCDMError opencdm_gstreamer_session_decrypt_buffer(struct OpenCDMSession* session, GstBuffer* buffer, GstCaps* caps)
{
    RDKPerf perf(__FUNCTION__);
    std::vector<uint8_t> keyId;
    copyKeyIdFromProtectionMeta(buffer, keyId);

    return decryptWithOutputRestrictedRetry(session, keyId, [=]() {
        return opencdm_gstreamer_session_decrypt_buffer_once(session, buffer, caps);
    });
}

#ifdef ENABLE_MULTI_DECRYPT
OpenCDMError opencdm_gstreamer_session_decrypt_buffer_multi(struct OpenCDMSession* session, GstBuffer* buffers[], const uint16_t count, GstCaps* caps)
{
    RDKPerf perf(__FUNCTION__);
    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);

    if (!gst_svp_is_multiple_decrypt_supported()) {
        // Reported once; the capability is fixed when libgstsvpext is loaded
        static std::atomic_flag reported = ATOMIC_FLAG_INIT;
        if (!reported.test_and_set()) {
            LOGDECRYPT(ERROR, "Batch decrypt unavailable: libocdm was built with "
                              "ENABLE_MULTI_DECRYPT but this platform's gst-svp-ext provides no "
                              "gst_buffer_vector_append_svp_transform() implementation. All "
                              "batch decrypt requests will fail.");
        }
        return ERROR_METHOD_NOT_IMPLEMENTED;
    }

    if ((count > 0) && (buffers != nullptr)) {
        std::vector<uint8_t> keyId;
        for (uint16_t buffIdx = 0; buffIdx < count; ++buffIdx) {
           if (copyKeyIdFromProtectionMeta(buffers[buffIdx], keyId)) {
                break;
            }
        }

        result = decryptWithOutputRestrictedRetry(session, keyId, [=]() {
            return opencdm_gstreamer_session_decrypt_buffer_multi_once(session, buffers, count, caps);
        });
    }
    return result;
}
#endif // ENABLE_MULTI_DECRYPT
