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
#include "open_cdm.h"

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

void ResetAdapterMocks()
{
    g_gstBufferMapSucceeds  = false;
    std::memset(g_mockKeyIdData, 0, sizeof(g_mockKeyIdData));
    g_mockKeyIdSize         = 0;
    g_keyStatusSequence     = { Usable };
    s_keyStatusIdx          = 0;
    g_mockDecryptOnceResult = ERROR_NONE;
}

// ── GStreamer function stubs ──────────────────────────────────────────────────

extern "C" {

gboolean gst_buffer_map(GstBuffer* buffer, GstMapInfo* info, GstMapFlags)
{
    if (buffer == nullptr || info == nullptr || !g_gstBufferMapSucceeds)
        return FALSE;
    info->data = g_mockKeyIdData;
    info->size = static_cast<gsize>(g_mockKeyIdSize);
    return TRUE;
}

void gst_buffer_unmap(GstBuffer*, GstMapInfo* info)
{
    if (info != nullptr) {
        info->data = nullptr;
        info->size = 0;
    }
}

GstProtectionMeta* gst_buffer_get_protection_meta(GstBuffer*)
{
    return nullptr;
}

const GValue* gst_structure_get_value(const GstStructure*, const gchar*)
{
    return nullptr;
}

GstBuffer* gst_value_get_buffer(const GValue*)
{
    return nullptr;
}

// opencdm_session_status has C linkage (declared extern "C" in open_cdm.h)
KeyStatus opencdm_session_status(const struct OpenCDMSession* session,
                                  const uint8_t[], uint8_t)
{
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
    return g_mockDecryptOnceResult;
}
} // extern "C"

OpenCDMError opencdm_gstreamer_session_decrypt_once(
    struct OpenCDMSession* session,
    GstBuffer*, GstBuffer*, uint32_t, GstBuffer*, GstBuffer*, uint32_t)
{
    if (session == nullptr)
        return ERROR_INVALID_SESSION;
    return g_mockDecryptOnceResult;
}

