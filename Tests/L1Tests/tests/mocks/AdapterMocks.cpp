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

#include "gst/gst.h"
#include "gst_svp_meta.h"
#include "open_cdm_adapter.h"

#include <cstring>
#include <vector>

// ── configurable mock state ───────────────────────────────────────────────────

bool     g_gstBufferMapSucceeds  = false;
uint8_t  g_mockKeyIdData[16]     = {};
size_t   g_mockKeyIdSize         = 0;

// opencdm_session_status returns values from this queue in call order;
// after the queue is exhausted the last entry is repeated.
std::vector<KeyStatus> g_keyStatusSequence = { Usable };
static size_t          s_keyStatusIdx      = 0;

OpenCDMError g_mockDecryptOnceResult = ERROR_NONE;
std::vector<OpenCDMError> g_mockDecryptOnceResults;
int g_decryptOnceCalls = 0;
int g_keyStatusCalls = 0;
int g_bufferMapCalls = 0;
int g_bufferUnmapCalls = 0;
GstBuffer* g_protectedBuffer = nullptr;
GstProtectionMeta* g_mockProtectionMeta = nullptr;
const GValue* g_mockKidValue = nullptr;
GstBuffer* g_mockKidBuffer = nullptr;

#ifdef ENABLE_MULTI_DECRYPT
bool g_multipleDecryptSupported = true;
int g_multiDecryptCalls = 0;
GstBuffer** g_lastMultiBuffers = nullptr;
uint16_t g_lastMultiCount = 0;
GstCaps* g_lastMultiCaps = nullptr;
#endif

static OpenCDMError nextDecryptResult()
{
    const size_t index = static_cast<size_t>(g_decryptOnceCalls++);
    if (g_mockDecryptOnceResults.empty()) {
        return g_mockDecryptOnceResult;
    }
    return g_mockDecryptOnceResults[
        index < g_mockDecryptOnceResults.size()
            ? index : g_mockDecryptOnceResults.size() - 1];
}

void ResetAdapterMocks()
{
    g_gstBufferMapSucceeds  = false;
    std::memset(g_mockKeyIdData, 0, sizeof(g_mockKeyIdData));
    g_mockKeyIdSize         = 0;
    g_keyStatusSequence     = { Usable };
    s_keyStatusIdx          = 0;
    g_mockDecryptOnceResult = ERROR_NONE;
    g_mockDecryptOnceResults.clear();
    g_decryptOnceCalls = 0;
    g_keyStatusCalls = 0;
    g_bufferMapCalls = 0;
    g_bufferUnmapCalls = 0;
    g_protectedBuffer = nullptr;
    g_mockProtectionMeta = nullptr;
    g_mockKidValue = nullptr;
    g_mockKidBuffer = nullptr;
#ifdef ENABLE_MULTI_DECRYPT
    g_multipleDecryptSupported = true;
    g_multiDecryptCalls = 0;
    g_lastMultiBuffers = nullptr;
    g_lastMultiCount = 0;
    g_lastMultiCaps = nullptr;
#endif
}

// ── GStreamer function stubs ──────────────────────────────────────────────────

extern "C" {

gboolean gst_buffer_map(GstBuffer* buffer, GstMapInfo* info, GstMapFlags)
{
    ++g_bufferMapCalls;
    if (buffer == nullptr || info == nullptr || !g_gstBufferMapSucceeds)
        return FALSE;
    info->data = g_mockKeyIdData;
    info->size = static_cast<gsize>(g_mockKeyIdSize);
    return TRUE;
}

void gst_buffer_unmap(GstBuffer*, GstMapInfo* info)
{
    ++g_bufferUnmapCalls;
    if (info != nullptr) {
        info->data = nullptr;
        info->size = 0;
    }
}

GstProtectionMeta* gst_buffer_get_protection_meta(GstBuffer* buffer)
{
    return buffer != nullptr && buffer == g_protectedBuffer
        ? g_mockProtectionMeta : nullptr;
}

const GValue* gst_structure_get_value(const GstStructure*, const gchar* field)
{
    return std::strcmp(field, "kid") == 0 ? g_mockKidValue : nullptr;
}

GstBuffer* gst_value_get_buffer(const GValue*)
{
    return g_mockKidBuffer;
}

// opencdm_session_status has C linkage (declared extern "C" in open_cdm.h)
KeyStatus opencdm_session_status(const struct OpenCDMSession* session,
                                  const uint8_t[], uint8_t)
{
    ++g_keyStatusCalls;
    if (session == nullptr)
        return InternalError;
    const KeyStatus status =
        (s_keyStatusIdx < g_keyStatusSequence.size())
        ? g_keyStatusSequence[s_keyStatusIdx]
        : g_keyStatusSequence.back();
    if (s_keyStatusIdx < g_keyStatusSequence.size())
        ++s_keyStatusIdx;
    return status;
}

} // extern "C"

// ── _once stubs (replaces open_cdm_adapter.cpp at link time) ─────────────────
// opencdm_gstreamer_session_decrypt_buffer_once: C linkage (declared in
//   open_cdm_adapter.h extern "C" block).
// opencdm_gstreamer_session_decrypt_once: C++ linkage (forward-declared
//   without extern "C" inside open_cdm_decrypt.cpp, called from a lambda).

extern "C" {
OpenCDMError opencdm_gstreamer_session_decrypt_buffer_once(
    struct OpenCDMSession* session, GstBuffer*, GstCaps*)
{
    if (session == nullptr)
        return ERROR_INVALID_SESSION;
    return nextDecryptResult();
}
} // extern "C"

OpenCDMError opencdm_gstreamer_session_decrypt_once(
    struct OpenCDMSession* session,
    GstBuffer*, GstBuffer*, uint32_t, GstBuffer*, GstBuffer*, uint32_t)
{
    if (session == nullptr)
        return ERROR_INVALID_SESSION;
    return nextDecryptResult();
}

#ifdef ENABLE_MULTI_DECRYPT
bool gst_svp_is_multiple_decrypt_supported()
{
    return g_multipleDecryptSupported;
}

extern "C" OpenCDMError opencdm_gstreamer_session_decrypt_buffer_multi_once(
    struct OpenCDMSession* session, GstBuffer* buffers[],
    const uint16_t count, GstCaps* caps)
{
    ++g_multiDecryptCalls;
    g_lastMultiBuffers = buffers;
    g_lastMultiCount = count;
    g_lastMultiCaps = caps;
    if (session == nullptr) {
        return ERROR_INVALID_SESSION;
    }
    return nextDecryptResult();
}
#endif

