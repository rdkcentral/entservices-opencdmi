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
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "open_cdm.h"
#include "open_cdm_ext.h"
#define private public
#include "open_cdm_impl.h"
#undef private

uint32_t opencdm_session_get_session_id_ext(struct OpenCDMSession* opencdmSession);
OpenCDMError opencdm_destruct_session_ext(OpenCDMSession* opencdmSession);

namespace {

class FakeSessionExt : public Exchange::ISessionExt {
public:
    uint32_t sessionIdExtValue = 1234;
    Exchange::OCDM_RESULT setDrmHeaderResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    Exchange::OCDM_RESULT getChallengeDataResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    Exchange::OCDM_RESULT cancelChallengeDataResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    Exchange::OCDM_RESULT storeLicenseDataResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    Exchange::OCDM_RESULT selectKeyIdResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    Exchange::OCDM_RESULT cleanDecryptContextResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    std::string bufferIdExtValue = "buffer-ext";
    std::vector<uint8_t> challengeData;
    uint16_t challengeDataLengthOut = 0;
    uint32_t lastChallengeIsLdl = 0;
    std::vector<uint8_t> lastDrmHeader;
    std::vector<uint8_t> lastLicenseData;
    std::vector<uint8_t> lastSelectedKeyId;
    uint8_t lastSelectedKeyLength = 0;
    std::vector<uint8_t> secureStopIdData;

    uint32_t AddRef() const override
    {
        return Core::ERROR_NONE;
    }

    uint32_t Release() const override
    {
        return Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t interfaceNumber) override
    {
        if ((interfaceNumber == Exchange::ISessionExt::ID) ||
            (interfaceNumber == Core::IUnknown::ID)) {
            return static_cast<Exchange::ISessionExt*>(this);
        }
        return nullptr;
    }

    uint32_t SessionIdExt() const override
    {
        return sessionIdExtValue;
    }

    std::string BufferIdExt() const override
    {
        return bufferIdExtValue;
    }

    Exchange::OCDM_RESULT SetDrmHeader(const uint8_t drmHeader[], uint16_t drmHeaderLength) override
    {
        lastDrmHeader.assign(drmHeader, drmHeader + drmHeaderLength);
        return setDrmHeaderResult;
    }

    Exchange::OCDM_RESULT GetChallengeDataExt(uint8_t* challenge,
                                              uint16_t& challengeSize,
                                              uint32_t isLDL) override
    {
        lastChallengeIsLdl = isLDL;
        const uint16_t copyLength = static_cast<uint16_t>(
            std::min<size_t>(challengeSize, challengeData.size()));
        if ((challenge != nullptr) && (copyLength > 0)) {
            std::copy(challengeData.begin(), challengeData.begin() + copyLength,
                      challenge);
        }
        challengeSize = challengeDataLengthOut;
        return getChallengeDataResult;
    }

    Exchange::OCDM_RESULT CancelChallengeDataExt() override
    {
        return cancelChallengeDataResult;
    }

    Exchange::OCDM_RESULT StoreLicenseData(const uint8_t licenseData[],
                                           uint16_t licenseDataSize,
                                           uint8_t* secureStopId) override
    {
        lastLicenseData.assign(licenseData, licenseData + licenseDataSize);
        if ((secureStopId != nullptr) && !secureStopIdData.empty()) {
            std::copy(secureStopIdData.begin(), secureStopIdData.end(), secureStopId);
        }
        return storeLicenseDataResult;
    }

    Exchange::OCDM_RESULT SelectKeyId(const uint8_t keyLength,
                                      const uint8_t keyId[]) override
    {
        lastSelectedKeyLength = keyLength;
        lastSelectedKeyId.assign(keyId, keyId + keyLength);
        return selectKeyIdResult;
    }

    Exchange::OCDM_RESULT CleanDecryptContext() override
    {
        return cleanDecryptContextResult;
    }
};

TEST(ClientOpenCdmCoreApiL1Tests, DestructSystemRejectsNull)
{
    EXPECT_EQ(ERROR_INVALID_ARG, opencdm_destruct_system(nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, SupportedRobustnessValidatesArguments)
{
    OpenCDMSystem system("com.widevine.alpha", "meta");
    char** robustness = nullptr;
    uint16_t count = 0;

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_supported_robustness(nullptr, &robustness, &count));
    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_supported_robustness(&system, nullptr, &count));
    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_supported_robustness(&system, &robustness, nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, IsTypeSupportedReturnsNonSuccessWithoutService)
{
    const OpenCDMError result =
        opencdm_is_type_supported("com.widevine.alpha", "video/mp4");

    EXPECT_NE(ERROR_NONE, result);
}

TEST(ClientOpenCdmCoreApiL1Tests, SystemMetadataBufferHandling)
{
    OpenCDMSystem system("com.widevine.alpha", "meta");

    uint16_t requiredSize = 0;
    EXPECT_EQ(ERROR_MORE_DATA_AVAILABLE,
              opencdm_system_get_metadata(&system, nullptr, &requiredSize));
    EXPECT_EQ(5u, requiredSize);

    char small[2] = {0};
    uint16_t smallSize = sizeof(small);
    EXPECT_EQ(ERROR_MORE_DATA_AVAILABLE,
              opencdm_system_get_metadata(&system, small, &smallSize));
    EXPECT_EQ(5u, smallSize);

    char exact[8] = {0};
    uint16_t exactSize = sizeof(exact);
    EXPECT_EQ(ERROR_NONE,
              opencdm_system_get_metadata(&system, exact, &exactSize));
    EXPECT_STREQ("meta", exact);
    EXPECT_EQ(5u, exactSize);
}

TEST(ClientOpenCdmCoreApiL1Tests, MetricSystemDataRejectsNullInputs)
{
    uint32_t length = 0;
    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_get_metric_system_data(nullptr, &length, nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionLookupFallbacks)
{
    const uint8_t keyId[2] = {0x01, 0x02};

    EXPECT_EQ(nullptr, opencdm_get_session(keyId, sizeof(keyId), 0));

    OpenCDMSystem system("com.widevine.alpha", "meta");
    EXPECT_EQ(nullptr,
              opencdm_get_system_session(&system, keyId, sizeof(keyId), 0));
}

TEST(ClientOpenCdmCoreApiL1Tests, ServerCertificateSupportIsFalse)
{
    EXPECT_EQ(OPENCDM_BOOL_FALSE,
              opencdm_system_supports_server_certificate(nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, ServerCertificateSetRejectsNullSystem)
{
    const uint8_t certificate[2] = {0xA0, 0xB0};

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_set_server_certificate(nullptr, certificate,
                                                    sizeof(certificate)));
}

TEST(ClientOpenCdmCoreApiL1Tests, ConstructSessionRejectsInvalidArgs)
{
    OpenCDMSession* session = nullptr;
    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_construct_session(nullptr, Temporary, "cenc", nullptr, 0,
                                        nullptr, 0, nullptr, nullptr, &session));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_construct_session(&system, Temporary, "cenc", nullptr, 0,
                                        nullptr, 0, nullptr, nullptr, nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionLifecycleApisRejectNullSession)
{
    EXPECT_EQ(ERROR_INVALID_SESSION, opencdm_destruct_session(nullptr));
    EXPECT_EQ(ERROR_INVALID_SESSION, opencdm_session_load(nullptr));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_metadata(nullptr, nullptr, nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionIdentityApisReturnDefaultsForNull)
{
    EXPECT_STREQ("", opencdm_session_id(nullptr));
    EXPECT_STREQ("", opencdm_session_buffer_id(nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionStateApisReturnDefaultsForNull)
{
    const uint8_t keyId[2] = {0x10, 0x11};

    EXPECT_EQ(0u, opencdm_session_has_key_id(nullptr, sizeof(keyId), keyId));
    EXPECT_EQ(KeyStatus::InternalError,
              opencdm_session_status(nullptr, keyId, sizeof(keyId)));
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
              opencdm_session_error(nullptr, keyId, sizeof(keyId)));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_system_error(nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionMutationApisRejectNullSession)
{
    const uint8_t message[2] = {0x01, 0x02};

    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_update(nullptr, message, sizeof(message)));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_remove(nullptr));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_set_parameter(nullptr, "name", "value"));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_resetoutputprotection(nullptr));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_close(nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, DecryptApisRejectNullSession)
{
    uint8_t encrypted[4] = {0};
    const uint8_t iv[16] = {0};
    const uint8_t keyId[16] = {0};
    EncryptionPattern pattern = {0, 0};

    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_decrypt(nullptr, encrypted, sizeof(encrypted),
                                      EncryptionScheme::AesCtr_Cenc,
                                      pattern, iv, sizeof(iv), keyId,
                                      sizeof(keyId), 0));

    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_decrypt_v2(nullptr, encrypted, sizeof(encrypted),
                                         nullptr, nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, MetricSessionDataRejectsNullSession)
{
    uint32_t length = 4;
    uint8_t buffer[4] = {0};

    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_get_metric_session_data(nullptr, &length, buffer));
}

TEST(ClientOpenCdmCoreApiL1Tests, DisposeCanBeCalled)
{
    opencdm_dispose();
    SUCCEED();
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionExtensionApisForwardToSessionExt)
{
    auto* fakeSession = new FakeSessionExt();
    fakeSession->challengeData = {0x11, 0x22, 0x33};
    fakeSession->challengeDataLengthOut = 3;
    fakeSession->secureStopIdData = {
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
    };
    auto* sessionStorage = ::operator new(sizeof(OpenCDMSession));
    auto* session = static_cast<OpenCDMSession*>(sessionStorage);
    session->_sessionExt = fakeSession;
    session->_refCount = 2;

    const uint8_t drmHeader[4] = {0x01, 0x02, 0x03, 0x04};
    uint8_t challenge[8] = {0};
    uint32_t challengeSize = sizeof(challenge);
    const uint8_t licenseData[3] = {0x61, 0x62, 0x63};
    uint8_t secureStopId[16] = {0};
    const uint8_t keyId[3] = {0x21, 0x22, 0x23};

    EXPECT_EQ(1234u, opencdm_session_get_session_id_ext(session));
    EXPECT_EQ(ERROR_NONE,
              opencdm_session_set_drm_header(session, drmHeader,
                                             sizeof(drmHeader)));
    EXPECT_EQ(drmHeader[0], fakeSession->lastDrmHeader[0]);
    EXPECT_EQ(sizeof(drmHeader), fakeSession->lastDrmHeader.size());

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_get_challenge_data(session, challenge,
                                                 &challengeSize, 7));
    EXPECT_EQ(3u, challengeSize);
    EXPECT_EQ(7u, fakeSession->lastChallengeIsLdl);
    EXPECT_EQ(0x11, challenge[0]);
    EXPECT_EQ(0x22, challenge[1]);
    EXPECT_EQ(0x33, challenge[2]);

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_cancel_challenge_data(session));

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_store_license_data(session, licenseData,
                                                 sizeof(licenseData),
                                                 secureStopId));
    EXPECT_EQ(sizeof(licenseData), fakeSession->lastLicenseData.size());
    EXPECT_EQ(0x61, fakeSession->lastLicenseData[0]);
    EXPECT_EQ(0xA0, secureStopId[0]);
    EXPECT_EQ(0xAF, secureStopId[15]);

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_select_key_id(session, sizeof(keyId), keyId));
    EXPECT_EQ(sizeof(keyId), fakeSession->lastSelectedKeyLength);
    EXPECT_EQ(0x21, fakeSession->lastSelectedKeyId[0]);
    EXPECT_EQ(0x23, fakeSession->lastSelectedKeyId[2]);

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_clean_decrypt_context(session));
    EXPECT_EQ(ERROR_NONE, opencdm_destruct_session_ext(session));

    delete fakeSession;
    ::operator delete(sessionStorage);
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionExtensionChallengeDataSupportsSizeProbe)
{
    auto* fakeSession = new FakeSessionExt();
    fakeSession->challengeData = {0x44, 0x55, 0x66};
    fakeSession->challengeDataLengthOut = 3;
    auto* sessionStorage = ::operator new(sizeof(OpenCDMSession));
    auto* session = static_cast<OpenCDMSession*>(sessionStorage);
    session->_sessionExt = fakeSession;
    session->_refCount = 2;

    uint32_t challengeSize = 0;

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_get_challenge_data(session, nullptr,
                                                 &challengeSize, 9));
    EXPECT_EQ(3u, challengeSize);
    EXPECT_EQ(9u, fakeSession->lastChallengeIsLdl);

    delete fakeSession;
    ::operator delete(sessionStorage);
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionExtensionApisHandleZeroOrShortInputs)
{
    auto* fakeSession = new FakeSessionExt();
    fakeSession->challengeData = {0x88, 0x99, 0xAA};
    fakeSession->challengeDataLengthOut = 3;
    auto* sessionStorage = ::operator new(sizeof(OpenCDMSession));
    auto* session = static_cast<OpenCDMSession*>(sessionStorage);
    session->_sessionExt = fakeSession;
    session->_refCount = 2;

    const uint8_t emptyHeader[1] = {0};
    const uint8_t keyId[1] = {0};
    uint8_t shortChallenge[1] = {0};
    uint32_t shortChallengeSize = sizeof(shortChallenge);

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_set_drm_header(session, emptyHeader, 0));
    EXPECT_TRUE(fakeSession->lastDrmHeader.empty());

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_select_key_id(session, 0, keyId));
    EXPECT_EQ(0u, fakeSession->lastSelectedKeyLength);
    EXPECT_TRUE(fakeSession->lastSelectedKeyId.empty());

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_get_challenge_data(session, shortChallenge,
                                                 &shortChallengeSize, 0));
    EXPECT_EQ(3u, shortChallengeSize);
    EXPECT_EQ(0x88, shortChallenge[0]);

    delete fakeSession;
    ::operator delete(sessionStorage);
}

} // namespace
