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

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "open_cdm_adapter.h"
#include "gst/gst.h"

// ── mock control (defined in AdapterMocks.cpp) ───────────────────────────────

extern bool                    g_gstBufferMapSucceeds;
extern uint8_t                 g_mockKeyIdData[16];
extern size_t                  g_mockKeyIdSize;
extern std::vector<KeyStatus>  g_keyStatusSequence;
extern OpenCDMError            g_mockDecryptOnceResult;
void ResetAdapterMocks();

namespace {

// Stable non-null pointer — adapter code only null-checks the session pointer
static int s_dummySession = 0;
OpenCDMSession* const kFakeSession =
    reinterpret_cast<OpenCDMSession*>(&s_dummySession);

class AdapterRdkTest : public ::testing::Test {
protected:
    void SetUp() override { ResetAdapterMocks(); }
};

// ── opencdm_gstreamer_session_decrypt ────────────────────────────────────────

TEST_F(AdapterRdkTest, DecryptNullSessionReturnsInvalidSession)
{
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_gstreamer_session_decrypt(
                  nullptr, nullptr, nullptr, 0, nullptr, nullptr, 0));
}

TEST_F(AdapterRdkTest, DecryptNullKeyIdBufferTakesDirectDecryptPath)
{
    // null keyID → copyKeyId returns false → keyId empty → decryptOnce called immediately
    g_mockDecryptOnceResult = ERROR_NONE;
    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, nullptr, 0));
}

TEST_F(AdapterRdkTest, DecryptWithUsableKeyStatusSucceeds)
{
    g_gstBufferMapSucceeds  = true;
    g_mockKeyIdSize         = 16;
    g_keyStatusSequence     = { Usable };
    g_mockDecryptOnceResult = ERROR_NONE;

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

TEST_F(AdapterRdkTest, DecryptWithStatusPendingIsConsideredUsable)
{
    g_gstBufferMapSucceeds  = true;
    g_mockKeyIdSize         = 16;
    g_keyStatusSequence     = { StatusPending };
    g_mockDecryptOnceResult = ERROR_NONE;

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

TEST_F(AdapterRdkTest, DecryptWithOutputDownscaledIsConsideredUsable)
{
    g_gstBufferMapSucceeds  = true;
    g_mockKeyIdSize         = 16;
    g_keyStatusSequence     = { OutputDownscaled };
    g_mockDecryptOnceResult = ERROR_NONE;

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

TEST_F(AdapterRdkTest, DecryptWithReleasedStatusAbortsImmediately)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize        = 16;
    g_keyStatusSequence    = { Released };  // non-usable, non-restricted → ERROR_FAIL

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_FAIL,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

TEST_F(AdapterRdkTest, DecryptWithExpiredStatusAbortsImmediately)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize        = 16;
    g_keyStatusSequence    = { Expired };

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_FAIL,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

TEST_F(AdapterRdkTest, DecryptWithInternalErrorStatusAbortsImmediately)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize        = 16;
    g_keyStatusSequence    = { InternalError };

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_FAIL,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

// OutputRestricted causes a 250 ms sleep before the next retry
TEST_F(AdapterRdkTest, DecryptWithOutputRestrictedThenUsableRetriesAndSucceeds)
{
    g_gstBufferMapSucceeds  = true;
    g_mockKeyIdSize         = 16;
    // First status call → OutputRestricted (sleep 250 ms), second → Usable
    g_keyStatusSequence     = { OutputRestricted, Usable };
    g_mockDecryptOnceResult = ERROR_NONE;

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

TEST_F(AdapterRdkTest, DecryptOnceFail_PostStatusNotRestricted_ReturnsLastError)
{
    g_gstBufferMapSucceeds  = true;
    g_mockKeyIdSize         = 16;
    // preDecrypt = Usable, decryptOnce fails, postDecrypt = Usable (not restricted)
    g_keyStatusSequence     = { Usable, Usable };
    g_mockDecryptOnceResult = ERROR_FAIL;

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_FAIL,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

TEST_F(AdapterRdkTest, DecryptWithHwErrorStatusAbortsImmediately)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize        = 16;
    g_keyStatusSequence    = { HWError };

    GstBuffer keyIdBuf;
    EXPECT_EQ(ERROR_FAIL,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyIdBuf, 0));
}

// ── opencdm_gstreamer_session_decrypt_buffer ─────────────────────────────────

TEST_F(AdapterRdkTest, DecryptBufferNullSessionReturnsInvalidSession)
{
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_gstreamer_session_decrypt_buffer(nullptr, nullptr, nullptr));
}

TEST_F(AdapterRdkTest, DecryptBufferNullBufferHasNoProtectionMetaTakesDirectPath)
{
    // null buffer → copyKeyIdFromProtectionMeta returns false → keyId empty → fast path
    g_mockDecryptOnceResult = ERROR_NONE;
    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt_buffer(
                  kFakeSession, nullptr, nullptr));
}

TEST_F(AdapterRdkTest, DecryptBufferWithBufferButNoProtectionMetaTakesDirectPath)
{
    // gst_buffer_get_protection_meta stub returns nullptr → keyId empty → fast path
    g_mockDecryptOnceResult = ERROR_NONE;
    GstBuffer buf;
    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt_buffer(
                  kFakeSession, &buf, nullptr));
}

} // namespace
