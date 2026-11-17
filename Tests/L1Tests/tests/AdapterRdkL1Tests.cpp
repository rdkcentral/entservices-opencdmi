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

#include <chrono>
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
extern std::vector<OpenCDMError> g_mockDecryptOnceResults;
extern int g_decryptOnceCalls;
extern int g_keyStatusCalls;
extern int g_bufferMapCalls;
extern int g_bufferUnmapCalls;
extern GstBuffer* g_protectedBuffer;
extern GstProtectionMeta* g_mockProtectionMeta;
extern const GValue* g_mockKidValue;
extern GstBuffer* g_mockKidBuffer;
#ifdef ENABLE_MULTI_DECRYPT
extern bool g_multipleDecryptSupported;
extern int g_multiDecryptCalls;
extern GstBuffer** g_lastMultiBuffers;
extern uint16_t g_lastMultiCount;
extern GstCaps* g_lastMultiCaps;
#endif
void ResetAdapterMocks();

namespace {

// Stable non-null pointer — adapter code only null-checks the session pointer
static int s_dummySession = 0;
OpenCDMSession* const kFakeSession =
    reinterpret_cast<OpenCDMSession*>(&s_dummySession);

class AdapterRdkTest : public ::testing::Test {
protected:
    void SetUp() override { ResetAdapterMocks(); }

    void SetProtectionMeta(GstBuffer* buffer)
    {
        g_protectedBuffer = buffer;
        g_mockProtectionMeta = &protectionMeta;
        g_mockKidValue = &kidValue;
        g_mockKidBuffer = &kidBuffer;
        g_gstBufferMapSucceeds = true;
        g_mockKeyIdSize = 16;
    }

    GstStructure structure {};
    GstProtectionMeta protectionMeta { &structure };
    GValue kidValue {};
    GstBuffer kidBuffer {};
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
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(1, g_keyStatusCalls);
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
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(1, g_keyStatusCalls);
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
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(0, g_keyStatusCalls);
    EXPECT_EQ(0, g_bufferMapCalls);
}

TEST_F(AdapterRdkTest, DecryptFailedKeyIdMapReturnsDecryptErrorWithoutStatusQuery)
{
    g_mockDecryptOnceResult = ERROR_KEYSYSTEM_NOT_SUPPORTED;
    GstBuffer keyId {};

    EXPECT_EQ(ERROR_KEYSYSTEM_NOT_SUPPORTED,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyId, 0));
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(0, g_keyStatusCalls);
    EXPECT_EQ(1, g_bufferMapCalls);
    EXPECT_EQ(0, g_bufferUnmapCalls);
}

TEST_F(AdapterRdkTest, DecryptEmptyMappedKeyIdSkipsStatusAndUnmaps)
{
    g_gstBufferMapSucceeds = true;
    GstBuffer keyId {};

    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyId, 0));
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(0, g_keyStatusCalls);
    EXPECT_EQ(1, g_bufferUnmapCalls);
}

TEST_F(AdapterRdkTest, DecryptHdcp22RestrictionThenUsableAttemptsOnlyOnce)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize = 16;
    g_keyStatusSequence = { OutputRestrictedHDCP22, Usable };
    GstBuffer keyId {};

    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyId, 0));
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(2, g_keyStatusCalls);
    EXPECT_EQ(1, g_bufferMapCalls);
    EXPECT_EQ(1, g_bufferUnmapCalls);
}

TEST_F(AdapterRdkTest, DecryptFailedAttemptThenRestrictedStatusRetriesAndSucceeds)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize = 16;
    g_keyStatusSequence = { Usable, OutputRestricted, Usable };
    g_mockDecryptOnceResults = { ERROR_FAIL, ERROR_NONE };
    GstBuffer keyId {};

    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyId, 0));
    EXPECT_EQ(2, g_decryptOnceCalls);
    EXPECT_EQ(3, g_keyStatusCalls);
    EXPECT_EQ(1, g_bufferMapCalls);
    EXPECT_EQ(1, g_bufferUnmapCalls);
}

TEST_F(AdapterRdkTest, DecryptFailedAttemptWithoutRestrictionPreservesExactError)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize = 16;
    g_keyStatusSequence = { Usable, Usable };
    g_mockDecryptOnceResult = ERROR_KEYSYSTEM_NOT_SUPPORTED;
    GstBuffer keyId {};

    EXPECT_EQ(ERROR_KEYSYSTEM_NOT_SUPPORTED,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyId, 0));
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(2, g_keyStatusCalls);
}

TEST_F(AdapterRdkTest, DecryptPermanentRestrictionStopsAtSixSecondBudget)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize = 16;
    g_keyStatusSequence = { OutputRestricted };
    GstBuffer keyId {};
    const auto started = std::chrono::steady_clock::now();

    EXPECT_EQ(ERROR_FAIL,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyId, 0));
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_GE(elapsed, std::chrono::seconds(6));
    EXPECT_LT(elapsed, std::chrono::seconds(9));
    EXPECT_EQ(0, g_decryptOnceCalls);
    EXPECT_GT(g_keyStatusCalls, 0);
    EXPECT_LE(g_keyStatusCalls, 24);
}

TEST_F(AdapterRdkTest, DecryptRestrictionAfterFailureTimesOutWithOriginalError)
{
    g_gstBufferMapSucceeds = true;
    g_mockKeyIdSize = 16;
    g_keyStatusSequence = { Usable, OutputRestrictedHDCP22 };
    g_mockDecryptOnceResult = ERROR_KEYSYSTEM_NOT_SUPPORTED;
    GstBuffer keyId {};
    const auto started = std::chrono::steady_clock::now();

    EXPECT_EQ(ERROR_KEYSYSTEM_NOT_SUPPORTED,
              opencdm_gstreamer_session_decrypt(
                  kFakeSession, nullptr, nullptr, 0, nullptr, &keyId, 0));
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_GE(elapsed, std::chrono::seconds(6));
    EXPECT_LT(elapsed, std::chrono::seconds(9));
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_LE(g_keyStatusCalls, 25);
}

TEST_F(AdapterRdkTest, DecryptBufferMissingKidValueDoesNotQueryStatus)
{
    GstBuffer buffer {};
    SetProtectionMeta(&buffer);
    g_mockKidValue = nullptr;
    g_mockDecryptOnceResult = ERROR_FAIL;

    EXPECT_EQ(ERROR_FAIL,
              opencdm_gstreamer_session_decrypt_buffer(
                  kFakeSession, &buffer, nullptr));
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(0, g_keyStatusCalls);
    EXPECT_EQ(0, g_bufferMapCalls);
}

TEST_F(AdapterRdkTest, DecryptBufferMissingKidBufferDoesNotQueryStatus)
{
    GstBuffer buffer {};
    SetProtectionMeta(&buffer);
    g_mockKidBuffer = nullptr;

    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt_buffer(
                  kFakeSession, &buffer, nullptr));
    EXPECT_EQ(1, g_decryptOnceCalls);
    EXPECT_EQ(0, g_keyStatusCalls);
    EXPECT_EQ(0, g_bufferMapCalls);
}

TEST_F(AdapterRdkTest, DecryptBufferProtectionMetaDrivesRetryAfterFailure)
{
    GstBuffer buffer {};
    SetProtectionMeta(&buffer);
    g_keyStatusSequence = { Usable, OutputRestricted, Usable };
    g_mockDecryptOnceResults = { ERROR_FAIL, ERROR_NONE };

    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt_buffer(
                  kFakeSession, &buffer, nullptr));
    EXPECT_EQ(2, g_decryptOnceCalls);
    EXPECT_EQ(3, g_keyStatusCalls);
    EXPECT_EQ(1, g_bufferMapCalls);
    EXPECT_EQ(1, g_bufferUnmapCalls);
}

#ifdef ENABLE_MULTI_DECRYPT
TEST_F(AdapterRdkTest, MultiDecryptUnsupportedCapabilityDoesNotAttemptDecrypt)
{
    g_multipleDecryptSupported = false;
    GstBuffer buffer {};
    GstBuffer* buffers[] = { &buffer };

    EXPECT_EQ(ERROR_METHOD_NOT_IMPLEMENTED,
              opencdm_gstreamer_session_decrypt_buffer_multi(
                  kFakeSession, buffers, 1, nullptr));
    EXPECT_EQ(0, g_multiDecryptCalls);
    EXPECT_EQ(0, g_keyStatusCalls);
    EXPECT_EQ(0, g_bufferMapCalls);
}

TEST_F(AdapterRdkTest, MultiDecryptRejectsEmptyBatchAndNullArray)
{
    GstBuffer buffer {};
    GstBuffer* buffers[] = { &buffer };

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_gstreamer_session_decrypt_buffer_multi(
                  kFakeSession, buffers, 0, nullptr));
    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_gstreamer_session_decrypt_buffer_multi(
                  kFakeSession, nullptr, 1, nullptr));
    EXPECT_EQ(0, g_multiDecryptCalls);
}

TEST_F(AdapterRdkTest, MultiDecryptNullSessionReturnsInvalidSession)
{
    GstBuffer buffer {};
    GstBuffer* buffers[] = { &buffer };

    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_gstreamer_session_decrypt_buffer_multi(
                  nullptr, buffers, 1, nullptr));
    EXPECT_EQ(1, g_multiDecryptCalls);
    EXPECT_EQ(0, g_decryptOnceCalls);
}

TEST_F(AdapterRdkTest, MultiDecryptWithoutProtectionMetaForwardsBatchAndError)
{
    GstBuffer first {}, second {};
    GstBuffer* buffers[] = { &first, &second };
    GstCaps caps {};
    g_mockDecryptOnceResult = ERROR_KEYSYSTEM_NOT_SUPPORTED;

    EXPECT_EQ(ERROR_KEYSYSTEM_NOT_SUPPORTED,
              opencdm_gstreamer_session_decrypt_buffer_multi(
                  kFakeSession, buffers, 2, &caps));
    EXPECT_EQ(1, g_multiDecryptCalls);
    EXPECT_EQ(buffers, g_lastMultiBuffers);
    EXPECT_EQ(2, g_lastMultiCount);
    EXPECT_EQ(&caps, g_lastMultiCaps);
    EXPECT_EQ(0, g_keyStatusCalls);
}

TEST_F(AdapterRdkTest, MultiDecryptFindsKidInSecondBufferAndRetriesWholeBatch)
{
    GstBuffer first {}, second {};
    GstBuffer* buffers[] = { &first, &second };
    SetProtectionMeta(&second);
    g_keyStatusSequence = { Usable, OutputRestrictedHDCP22, Usable };
    g_mockDecryptOnceResults = { ERROR_FAIL, ERROR_NONE };

    EXPECT_EQ(ERROR_NONE,
              opencdm_gstreamer_session_decrypt_buffer_multi(
                  kFakeSession, buffers, 2, nullptr));
    EXPECT_EQ(2, g_multiDecryptCalls);
    EXPECT_EQ(3, g_keyStatusCalls);
    EXPECT_EQ(1, g_bufferMapCalls);
    EXPECT_EQ(1, g_bufferUnmapCalls);
    EXPECT_EQ(buffers, g_lastMultiBuffers);
    EXPECT_EQ(2, g_lastMultiCount);
}
#endif

} // namespace
