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
#include <cstdlib>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "open_cdm.h"
#include "open_cdm_ext.h"
#define private public
#include "open_cdm_impl.h"
#include "FakeServerInterfaces.h"
#undef private

uint32_t opencdm_session_get_session_id_ext(struct OpenCDMSession* opencdmSession);
OpenCDMError opencdm_destruct_session_ext(OpenCDMSession* opencdmSession);

namespace {

struct CallbackCapture {
    int challengeCalls = 0;
    int keyUpdateCalls = 0;
    int errorCalls = 0;
    int keysUpdatedCalls = 0;
    OpenCDMSession* challengeSession = nullptr;
    OpenCDMSession* keyUpdateSession = nullptr;
    const OpenCDMSession* keysUpdatedSession = nullptr;
    std::string lastUrl;
    std::vector<uint8_t> lastChallenge;
    std::vector<uint8_t> lastKeyId;
    std::string lastErrorMessage;
};

void ProcessChallengeCallback(OpenCDMSession* session,
                              void* userData,
                              const char url[],
                              const uint8_t challenge[],
                              const uint16_t challengeLength)
{
    auto* capture = static_cast<CallbackCapture*>(userData);
    ASSERT_NE(nullptr, capture);
    capture->challengeCalls++;
    capture->challengeSession = session;
    capture->lastUrl = (url != nullptr ? url : "");
    capture->lastChallenge.assign(challenge, challenge + challengeLength);
}

void KeyUpdateCallback(OpenCDMSession* session,
                       void* userData,
                       const uint8_t keyId[],
                       const uint8_t length)
{
    auto* capture = static_cast<CallbackCapture*>(userData);
    ASSERT_NE(nullptr, capture);
    capture->keyUpdateCalls++;
    capture->keyUpdateSession = session;
    capture->lastKeyId.assign(keyId, keyId + length);
}

void ErrorMessageCallback(OpenCDMSession*,
                          void* userData,
                          const char message[])
{
    auto* capture = static_cast<CallbackCapture*>(userData);
    ASSERT_NE(nullptr, capture);
    capture->errorCalls++;
    capture->lastErrorMessage = (message != nullptr ? message : "");
}

void KeysUpdatedCallback(const OpenCDMSession* session,
                         void* userData)
{
    auto* capture = static_cast<CallbackCapture*>(userData);
    ASSERT_NE(nullptr, capture);
    capture->keysUpdatedCalls++;
    capture->keysUpdatedSession = session;
}

class ScopedFakeAccessor {
public:
    ScopedFakeAccessor()
    {
        InstallFakeAccessor(&accessor);
    }

    ~ScopedFakeAccessor()
    {
        UninstallFakeAccessor();
    }

    FakeOpenCDMAccessor accessor;
};

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

class FakeCoreSession : public Exchange::ISession {
public:
    Exchange::OCDM_RESULT loadResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    Exchange::OCDM_RESULT removeResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    Exchange::OCDM_RESULT metricResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    Exchange::ISession::KeyStatus statusResult = Exchange::ISession::Usable;
    bool loadCalled = false;
    bool updateCalled = false;
    bool removeCalled = false;
    bool closeCalled = false;
    bool resetOutputProtectionCalled = false;
    std::vector<uint8_t> lastUpdateMessage;
    std::vector<uint8_t> metricData;
    std::string metadataValue = "core-session-meta";
    std::vector<std::pair<std::string, std::string>> parameters;

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
        if ((interfaceNumber == Exchange::ISession::ID) ||
            (interfaceNumber == Core::IUnknown::ID)) {
            return static_cast<Exchange::ISession*>(this);
        }
        return nullptr;
    }

    Exchange::OCDM_RESULT Load() override
    {
        loadCalled = true;
        return loadResult;
    }

    void Update(const uint8_t* keyMessage, const uint16_t keyLength) override
    {
        updateCalled = true;
        lastUpdateMessage.clear();
        if ((keyMessage != nullptr) && (keyLength > 0)) {
            lastUpdateMessage.assign(keyMessage, keyMessage + keyLength);
        }
    }

    Exchange::OCDM_RESULT Remove() override
    {
        removeCalled = true;
        return removeResult;
    }

    std::string Metadata() const override
    {
        return metadataValue;
    }

    Exchange::OCDM_RESULT Metricdata(uint32_t& bufferSize,
                                     uint8_t buffer[]) const override
    {
        const uint32_t copyLength = std::min(bufferSize,
            static_cast<uint32_t>(metricData.size()));
        if ((buffer != nullptr) && (copyLength > 0)) {
            std::copy(metricData.begin(), metricData.begin() + copyLength,
                      buffer);
        }
        bufferSize = static_cast<uint32_t>(metricData.size());
        return metricResult;
    }

    Exchange::ISession::KeyStatus Status() const override
    {
        return statusResult;
    }

    Exchange::ISession::KeyStatus Status(const uint8_t[],
                                         const uint8_t) const override
    {
        return statusResult;
    }

    Exchange::OCDM_RESULT CreateSessionBuffer(std::string& bufferID) override
    {
        bufferID = "core-buffer";
        return Exchange::OCDM_RESULT::OCDM_SUCCESS;
    }

    std::string BufferId() const override
    {
        return "core-buffer";
    }

    std::string SessionId() const override
    {
        return "core-session";
    }

    void Close() override
    {
        closeCalled = true;
    }

    void ResetOutputProtection() override
    {
        resetOutputProtectionCalled = true;
    }

    void SetParameter(const std::string& name, const std::string& value) override
    {
        parameters.emplace_back(name, value);
    }

    void Revoke(Exchange::ISession::ICallback*) override
    {
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

TEST(ClientOpenCdmCoreApiL1Tests, IsTypeSupportedReturnsSuccessWithAccessor)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.isTypeSupportedValue = true;

    const OpenCDMError result =
        opencdm_is_type_supported("com.widevine.alpha", "video/mp4");

    EXPECT_EQ(ERROR_NONE, result);
}

TEST(ClientOpenCdmCoreApiL1Tests, IsTypeSupportedReturnsNotSupportedWhenAccessorRejectsType)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.isTypeSupportedValue = false;

    const OpenCDMError result =
        opencdm_is_type_supported("com.widevine.alpha", "video/mp4");

    EXPECT_EQ(ERROR_KEYSYSTEM_NOT_SUPPORTED, result);
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

TEST(ClientOpenCdmCoreApiL1Tests, MetricSystemDataForwardsToAccessor)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.metricSystemDataResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    scoped.accessor.metricSystemData = {0x9A, 0x9B};

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint8_t metric[4] = {0};
    uint32_t metricLength = sizeof(metric);

    EXPECT_EQ(ERROR_NONE,
              opencdm_get_metric_system_data(&system, &metricLength, metric));
    EXPECT_EQ(2u, metricLength);
    EXPECT_EQ(0x9A, metric[0]);
    EXPECT_EQ(0x9B, metric[1]);
}

TEST(ClientOpenCdmCoreApiL1Tests, MetricSystemDataPropagatesAccessorFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.metricSystemDataResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint8_t metric[4] = {0};
    uint32_t metricLength = sizeof(metric);

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_get_metric_system_data(&system, &metricLength, metric));
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

TEST(ClientOpenCdmCoreApiL1Tests, ServerCertificateSetForwardsToAccessor)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.serverCertificateResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t certificate[3] = {0xA0, 0xB0, 0xC0};

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_set_server_certificate(&system, certificate,
                                                    sizeof(certificate)));
    ASSERT_EQ(sizeof(certificate), scoped.accessor.lastServerCertificate.size());
    EXPECT_EQ(0xA0, scoped.accessor.lastServerCertificate[0]);
    EXPECT_EQ(0xC0, scoped.accessor.lastServerCertificate[2]);
}

TEST(ClientOpenCdmCoreApiL1Tests, ServerCertificateSetPropagatesAccessorFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.serverCertificateResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t certificate[2] = {0xA0, 0xB0};

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_system_set_server_certificate(&system, certificate,
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

TEST(ClientOpenCdmCoreApiL1Tests, ConstructSessionCreatesSessionWithAccessor)
{
    ScopedFakeAccessor scoped;

    auto* fakeSession = new FakeOpenCDMAccessor::FakeSession();
    fakeSession->AddRef();
    fakeSession->sessionIdValue = "session-created";
    scoped.accessor.sessionToCreate = fakeSession;

    OpenCDMSession* session = nullptr;
    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t initData[3] = {0x01, 0x02, 0x03};
    const uint8_t cdmData[2] = {0xA1, 0xA2};

    EXPECT_EQ(ERROR_NONE,
              opencdm_construct_session(&system, Temporary, "cenc", initData,
                                        sizeof(initData), cdmData,
                                        sizeof(cdmData), nullptr, nullptr,
                                        &session));
    ASSERT_NE(nullptr, session);
    EXPECT_STREQ("session-created", opencdm_session_id(session));
    EXPECT_STREQ("buffer-ext", opencdm_session_buffer_id(session));
    EXPECT_EQ("com.widevine.alpha", scoped.accessor.lastCreateSessionKeySystem);
    EXPECT_EQ(Temporary, scoped.accessor.lastCreateSessionLicenseType);
    EXPECT_EQ("cenc", scoped.accessor.lastCreateSessionInitDataType);
    ASSERT_EQ(sizeof(initData), scoped.accessor.lastCreateSessionInitData.size());
    EXPECT_EQ(0x01, scoped.accessor.lastCreateSessionInitData[0]);
    EXPECT_EQ(0x03, scoped.accessor.lastCreateSessionInitData[2]);
    ASSERT_EQ(sizeof(cdmData), scoped.accessor.lastCreateSessionCdmData.size());
    EXPECT_EQ(0xA1, scoped.accessor.lastCreateSessionCdmData[0]);
    EXPECT_EQ(0xA2, scoped.accessor.lastCreateSessionCdmData[1]);

    EXPECT_EQ(ERROR_NONE, opencdm_destruct_session(session));
    EXPECT_TRUE(fakeSession->revokeCalled);
    fakeSession->Release();
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionCallbacksPropagateMediaKeySessionEvents)
{
    ScopedFakeAccessor scoped;

    auto* fakeSession = new FakeOpenCDMAccessor::FakeSession();
    fakeSession->AddRef();
    fakeSession->sessionIdValue = "session-with-callbacks";
    scoped.accessor.sessionToCreate = fakeSession;

    CallbackCapture capture;
    OpenCDMSessionCallbacks callbacks = {};
    callbacks.process_challenge_callback = ProcessChallengeCallback;
    callbacks.key_update_callback = KeyUpdateCallback;
    callbacks.error_message_callback = ErrorMessageCallback;
    callbacks.keys_updated_callback = KeysUpdatedCallback;

    OpenCDMSession* session = nullptr;
    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t initData[2] = {0x0A, 0x0B};

    ASSERT_EQ(ERROR_NONE,
              opencdm_construct_session(&system, Temporary, "cenc", initData,
                                        sizeof(initData), nullptr, 0,
                                        &callbacks, &capture, &session));
    ASSERT_NE(nullptr, session);

    scoped.accessor.FireOnKeyMessage({0xAA, 0xBB}, "https://license.example");
    scoped.accessor.FireOnError(7, Exchange::OCDM_RESULT::OCDM_FAIL,
                                "error-from-server");
    scoped.accessor.FireOnKeyStatusUpdate({0x01, 0x02}, Exchange::ISession::StatusPending);
    scoped.accessor.FireOnKeyStatusUpdate({0x01, 0x02}, Exchange::ISession::Usable);
    scoped.accessor.FireOnKeyStatusesUpdated();

    EXPECT_EQ(1, capture.challengeCalls);
    EXPECT_EQ(session, capture.challengeSession);
    EXPECT_EQ("https://license.example", capture.lastUrl);
    ASSERT_EQ(2u, capture.lastChallenge.size());
    EXPECT_EQ(0xAA, capture.lastChallenge[0]);
    EXPECT_EQ(0xBB, capture.lastChallenge[1]);

    EXPECT_EQ(1, capture.errorCalls);
    EXPECT_EQ("error-from-server", capture.lastErrorMessage);

    EXPECT_EQ(1, capture.keyUpdateCalls);
    EXPECT_EQ(session, capture.keyUpdateSession);
    ASSERT_EQ(2u, capture.lastKeyId.size());
    EXPECT_EQ(0x01, capture.lastKeyId[0]);
    EXPECT_EQ(0x02, capture.lastKeyId[1]);

    EXPECT_EQ(1, capture.keysUpdatedCalls);
    EXPECT_EQ(session, capture.keysUpdatedSession);

    EXPECT_EQ(ERROR_NONE, opencdm_destruct_session(session));
    EXPECT_TRUE(fakeSession->revokeCalled);
    fakeSession->Release();
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

TEST(ClientOpenCdmCoreApiL1Tests, SessionCoreApisForwardToSessionInterface)
{
    ScopedFakeAccessor scoped;
    auto* fakeCoreSession = new FakeCoreSession();
    fakeCoreSession->loadResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeCoreSession->removeResult = Exchange::OCDM_RESULT::OCDM_S_FALSE;
    fakeCoreSession->metricResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeCoreSession->metricData = {0xD1, 0xD2};
    fakeCoreSession->metadataValue = "session-meta";

    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);
    session->_session = fakeCoreSession;
    session->_sessionExt = nullptr;
    session->_refCount = 2;
    session->_sysError = Exchange::OCDM_RESULT::OCDM_BUSY_CANNOT_INITIALIZE;
    session->_errorCode = 77;

    EXPECT_EQ(ERROR_NONE, opencdm_session_load(session));
    EXPECT_TRUE(fakeCoreSession->loadCalled);

    const uint8_t updateMessage[2] = {0xAA, 0xAB};
    EXPECT_EQ(ERROR_NONE,
              opencdm_session_update(session, updateMessage,
                                     sizeof(updateMessage)));
    EXPECT_TRUE(fakeCoreSession->updateCalled);
    ASSERT_EQ(sizeof(updateMessage), fakeCoreSession->lastUpdateMessage.size());
    EXPECT_EQ(updateMessage[0], fakeCoreSession->lastUpdateMessage[0]);

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_set_parameter(session, "quality", "uhd"));
    ASSERT_EQ(1u, fakeCoreSession->parameters.size());
    EXPECT_EQ("quality", fakeCoreSession->parameters[0].first);
    EXPECT_EQ("uhd", fakeCoreSession->parameters[0].second);

    EXPECT_EQ(ERROR_NONE, opencdm_session_resetoutputprotection(session));
    EXPECT_TRUE(fakeCoreSession->resetOutputProtectionCalled);
    EXPECT_EQ(ERROR_NONE, opencdm_session_close(session));
    EXPECT_TRUE(fakeCoreSession->closeCalled);

    uint8_t metric[4] = {0};
    uint32_t metricLength = sizeof(metric);
    EXPECT_EQ(ERROR_NONE,
              opencdm_get_metric_session_data(session, &metricLength, metric));
    EXPECT_EQ(2u, metricLength);
    EXPECT_EQ(0xD1, metric[0]);
    EXPECT_EQ(0xD2, metric[1]);

    char metadata[32] = {0};
    uint16_t metadataSize = sizeof(metadata);
    EXPECT_EQ(ERROR_NONE,
              opencdm_session_metadata(session, metadata, &metadataSize));
    EXPECT_STREQ("session-meta", metadata);

    const uint8_t keyId[2] = {0x41, 0x42};
    EXPECT_EQ(static_cast<uint32_t>(Exchange::OCDM_RESULT::OCDM_BUSY_CANNOT_INITIALIZE),
              opencdm_session_error(session, keyId, sizeof(keyId)));
    EXPECT_EQ(static_cast<OpenCDMError>(77),
              opencdm_session_system_error(session));

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_S_FALSE),
              opencdm_session_remove(session));
    EXPECT_TRUE(fakeCoreSession->removeCalled);

    delete fakeCoreSession;
    delete session;
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionExtensionApisForwardToSessionExt)
{
    ScopedFakeAccessor scoped;
    auto* fakeSession = new FakeSessionExt();
    fakeSession->challengeData = {0x11, 0x22, 0x33};
    fakeSession->challengeDataLengthOut = 3;
    fakeSession->secureStopIdData = {
        0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
    };
    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);
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
    delete session;
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionExtensionChallengeDataSupportsSizeProbe)
{
    ScopedFakeAccessor scoped;
    auto* fakeSession = new FakeSessionExt();
    fakeSession->challengeData = {0x44, 0x55, 0x66};
    fakeSession->challengeDataLengthOut = 3;
    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);
    session->_sessionExt = fakeSession;
    session->_refCount = 2;

    uint32_t challengeSize = 0;

    EXPECT_EQ(ERROR_NONE,
              opencdm_session_get_challenge_data(session, nullptr,
                                                 &challengeSize, 9));
    EXPECT_EQ(3u, challengeSize);
    EXPECT_EQ(9u, fakeSession->lastChallengeIsLdl);

    delete fakeSession;
    delete session;
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionHasKeyIdReturnsTrueForPresentKey)
{
    ScopedFakeAccessor scoped;
    const uint8_t keyData[16] = {
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
        0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0
    };
    const uint8_t absentKey[16] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
    };

    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);
    new (&session->_keyStatuses) std::list<Exchange::KeyId>();
    session->_keyStatuses.emplace_back(keyData, static_cast<uint8_t>(sizeof(keyData)));

    EXPECT_EQ(1u, opencdm_session_has_key_id(session, sizeof(keyData), keyData));
    EXPECT_EQ(0u, opencdm_session_has_key_id(session, sizeof(absentKey), absentKey));

    session->_keyStatuses.~list();
    delete session;
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionStatusReturnsPendingForKeyInList)
{
    ScopedFakeAccessor scoped;
    const uint8_t keyData[16] = {
        0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
        0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0
    };

    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);
    new (&session->_keyStatuses) std::list<Exchange::KeyId>();
    session->_keyStatuses.emplace_back(keyData, static_cast<uint8_t>(sizeof(keyData)));

    // A key in the list with default status returns StatusPending via CDMState conversion
    const KeyStatus status = opencdm_session_status(session, keyData, sizeof(keyData));
    EXPECT_NE(KeyStatus::InternalError, status);

    session->_keyStatuses.~list();
    delete session;
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionIdReturnsStoredSessionId)
{
    ScopedFakeAccessor scoped;
    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);
    new (&session->_sessionId) std::string("my-drm-session-001");

    EXPECT_STREQ("my-drm-session-001", opencdm_session_id(session));

    session->_sessionId.~basic_string();
    delete session;
}

TEST(ClientOpenCdmCoreApiL1Tests, SystemGetMetadataRejectsNullSystem)
{
    uint16_t size = 8;
    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_get_metadata(nullptr, nullptr, &size));
}

TEST(ClientOpenCdmCoreApiL1Tests, SystemGetMetadataRejectsNullMetadataSize)
{
    OpenCDMSystem system("com.widevine.alpha", "meta");
    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_get_metadata(&system, nullptr, nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionMetadataRejectsNullMetadataSize)
{
    ScopedFakeAccessor scoped;
    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_session_metadata(session, nullptr, nullptr));

    delete session;
}

TEST(ClientOpenCdmCoreApiL1Tests, MetricSystemDataRejectsNullBufferLength)
{
    OpenCDMSystem system("com.widevine.alpha", "meta");
    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_get_metric_system_data(&system, nullptr, nullptr));
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionHasKeyIdWithZeroKeyLengthReturnsFalse)
{
    ScopedFakeAccessor scoped;
    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);
    new (&session->_keyStatuses) std::list<Exchange::KeyId>();

    const uint8_t keyData[4] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(0u, opencdm_session_has_key_id(session, 0, keyData));

    session->_keyStatuses.~list();
    delete session;
}

TEST(ClientOpenCdmCoreApiL1Tests, SessionExtensionApisHandleZeroOrShortInputs)
{
    ScopedFakeAccessor scoped;
    auto* fakeSession = new FakeSessionExt();
    fakeSession->challengeData = {0x88, 0x99, 0xAA};
    fakeSession->challengeDataLengthOut = 3;
    OpenCDMSystem system("com.widevine.alpha", "meta");
    auto* session = new OpenCDMSession(&system, "cenc", nullptr, 0, nullptr, 0,
                                       Temporary, nullptr, nullptr);
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
    delete session;
}

} // namespace
