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
#include <string>
#include <vector>

#include "open_cdm_impl.h"
#include "open_cdm.h"
#include "open_cdm_ext.h"
#include "FakeServerInterfaces.h"

OpenCDMError opencdm_parse_keysystem(std::string& keySystemDomain);
uint32_t opencdm_session_get_session_id_ext(struct OpenCDMSession* opencdmSession);
OpenCDMError opencdm_destruct_session_ext(OpenCDMSession* opencdmSession);

namespace {

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

TEST(ClientOpenCdmExtL1Tests, ParseKeySystemWithoutDomainKeepsOriginal)
{
    std::string keySystem("com.widevine.alpha");

    const OpenCDMError result = opencdm_parse_keysystem(keySystem);

    EXPECT_EQ(ERROR_NONE, result);
    EXPECT_EQ("com.widevine.alpha", keySystem);
}

TEST(ClientOpenCdmExtL1Tests, ParseKeySystemWithUnknownDomainFallsBackToBase)
{
    std::string keySystem("com.widevine.alpha;origin=unknown.domain.example");

    const OpenCDMError result = opencdm_parse_keysystem(keySystem);

    EXPECT_EQ(ERROR_NONE, result);
    EXPECT_EQ("com.widevine.alpha", keySystem);
}

TEST(ClientOpenCdmExtL1Tests, GetPropertiesParsesValidJson)
{
    PlayLevels levels {};

    const char* json =
        "{"
        "\"compressed-video\":10,"
        "\"uncompressed-video\":20,"
        "\"analog-video\":30,"
        "\"compressed-audio\":40,"
        "\"uncompressed-audio\":50,"
        "\"max-decode-width\":1920,"
        "\"max-decode-height\":1080"
        "}";

    const OpenCDMError result = opencdm_system_ext_get_properties(&levels, json);

    EXPECT_EQ(ERROR_NONE, result);
    EXPECT_EQ(10, levels._compressedDigitalVideoLevel);
    EXPECT_EQ(20, levels._uncompressedDigitalVideoLevel);
    EXPECT_EQ(30, levels._analogVideoLevel);
    EXPECT_EQ(40, levels._compressedDigitalAudioLevel);
    EXPECT_EQ(50, levels._uncompressedDigitalAudioLevel);
    EXPECT_EQ(1920u, levels._maxResDecodeWidth);
    EXPECT_EQ(1080u, levels._maxResDecodeHeight);
}

TEST(ClientOpenCdmExtL1Tests, GetPropertiesRejectsNullJson)
{
    PlayLevels levels {};

    const OpenCDMError result = opencdm_system_ext_get_properties(&levels, nullptr);

    EXPECT_EQ(ERROR_INVALID_ACCESSOR, result);
}

TEST(ClientOpenCdmExtL1Tests, GetPropertiesWithMalformedJsonKeepsDefaultLevels)
{
    PlayLevels levels {};

    const OpenCDMError result =
        opencdm_system_ext_get_properties(&levels, "{invalid-json");

    EXPECT_EQ(ERROR_NONE, result);
    EXPECT_EQ(0, levels._compressedDigitalVideoLevel);
    EXPECT_EQ(0, levels._uncompressedDigitalVideoLevel);
    EXPECT_EQ(0, levels._analogVideoLevel);
    EXPECT_EQ(0, levels._compressedDigitalAudioLevel);
    EXPECT_EQ(0, levels._uncompressedDigitalAudioLevel);
    EXPECT_EQ(0u, levels._maxResDecodeWidth);
    EXPECT_EQ(0u, levels._maxResDecodeHeight);
}

TEST(ClientOpenCdmExtL1Tests, ExtensionApisReturnInvalidAccessorWhenServerUnavailable)
{
    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint32_t ldlLimit = 0;
    uint8_t ids[32] = {0};
    uint32_t count = 0;
    uint8_t secureStop[64] = {0};
    uint16_t secureStopSize = sizeof(secureStop);
    uint8_t response[16] = {0};
    uint64_t drmTime = 0;
    uint8_t keyHash[64] = {0};
    uint8_t storeHash[64] = {0};
    char version[64] = {0};

    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_get_version(&system, version));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_get_drm_time(&system, &drmTime));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_ext_get_ldl_session_limit(&system, &ldlLimit));
    EXPECT_EQ(static_cast<uint32_t>(ERROR_INVALID_ACCESSOR),
              opencdm_system_ext_is_secure_stop_enabled(&system));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_ext_enable_secure_stop(&system, 1));
    EXPECT_EQ(static_cast<uint32_t>(ERROR_INVALID_ACCESSOR),
              opencdm_system_ext_reset_secure_stop(&system));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_ext_get_secure_stop_ids(&system, ids,
                                                     sizeof(ids), &count));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_ext_get_secure_stop(&system, ids, sizeof(ids),
                                                 secureStop, &secureStopSize));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_ext_commit_secure_stop(&system, ids, sizeof(ids),
                                                    response, sizeof(response)));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_delete_key_store(&system));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_delete_secure_store(&system));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_get_key_store_hash_ext(&system, keyHash, sizeof(keyHash)));
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_get_secure_store_hash_ext(&system, storeHash,
                                                sizeof(storeHash)));
}

TEST(ClientOpenCdmExtL1Tests, CreateSystemExtendedUsesAccessorMetadata)
{
    FakeOpenCDMAccessor fakeAccessor;
    fakeAccessor.metadataResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.metadataValue = "meta-value";
    InstallFakeAccessor(&fakeAccessor);

    OpenCDMSystem* system = nullptr;
    const OpenCDMError result =
        opencdm_create_system_extended("com.widevine.alpha;origin=unknown.site",
                                       &system);

    ASSERT_EQ(ERROR_NONE, result);
    ASSERT_NE(nullptr, system);
    EXPECT_EQ("com.widevine.alpha", system->keySystem());
    EXPECT_EQ("com.widevine.alpha", fakeAccessor.lastMetadataKeySystem);
    EXPECT_EQ("meta-value", system->Metadata());

    delete system;
    UninstallFakeAccessor();
}

TEST(ClientOpenCdmExtL1Tests, CreateSystemExtendedReturnsFailureWhenMetadataFails)
{
    FakeOpenCDMAccessor fakeAccessor;
    fakeAccessor.metadataResult = Exchange::OCDM_RESULT::OCDM_S_FALSE;
    InstallFakeAccessor(&fakeAccessor);

    OpenCDMSystem* system = nullptr;
    const OpenCDMError result =
        opencdm_create_system_extended("com.widevine.alpha", &system);

    EXPECT_NE(ERROR_NONE, result);
    EXPECT_EQ(nullptr, system);

    UninstallFakeAccessor();
}

TEST(ClientOpenCdmExtL1Tests, CreateSystemUsesExtendedPath)
{
    FakeOpenCDMAccessor fakeAccessor;
    fakeAccessor.metadataResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.metadataValue = "legacy-meta";
    InstallFakeAccessor(&fakeAccessor);

    OpenCDMSystem* system = opencdm_create_system("com.widevine.alpha");

    ASSERT_NE(nullptr, system);
    EXPECT_EQ("com.widevine.alpha", system->keySystem());
    EXPECT_EQ("legacy-meta", system->Metadata());

    delete system;
    UninstallFakeAccessor();
}

TEST(ClientOpenCdmExtL1Tests, SessionExtensionApisRejectNullSession)
{
    uint8_t challenge[4] = {0};
    uint32_t challengeSize = sizeof(challenge);
    uint8_t secureStopId[16] = {0};
    const uint8_t drmHeader[2] = {0x01, 0x02};
    const uint8_t licenseData[2] = {0x03, 0x04};
    const uint8_t keyId[2] = {0x05, 0x06};

    EXPECT_EQ(static_cast<uint32_t>(ERROR_INVALID_SESSION),
              opencdm_session_get_session_id_ext(nullptr));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_destruct_session_ext(nullptr));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_set_drm_header(nullptr, drmHeader,
                                             sizeof(drmHeader)));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_get_challenge_data(nullptr, challenge,
                                                 &challengeSize, 0));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_cancel_challenge_data(nullptr));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_store_license_data(nullptr, licenseData,
                                                 sizeof(licenseData),
                                                 secureStopId));
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_select_key_id(nullptr, sizeof(keyId), keyId));
    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_session_clean_decrypt_context(nullptr));
}

TEST(ClientOpenCdmExtL1Tests, ExtensionApisForwardToAccessor)
{
    FakeOpenCDMAccessor fakeAccessor;
    fakeAccessor.versionValue = "4.4.1";
    fakeAccessor.drmSystemTime = 987654;
    fakeAccessor.ldlSessionLimit = 9;
    fakeAccessor.secureStopEnabled = true;
    fakeAccessor.enableSecureStopResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.resetSecureStopsValue = 3;
    fakeAccessor.secureStopIdsResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.secureStopCount = 2;
    fakeAccessor.secureStopIdsData = {0x10, 0x20, 0x30, 0x40};
    fakeAccessor.getSecureStopResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.secureStopRawSize = 3;
    fakeAccessor.secureStopRawData = {0xAA, 0xBB, 0xCC};
    fakeAccessor.commitSecureStopResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.deleteKeyStoreResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.deleteSecureStoreResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.keyStoreHashResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.secureStoreHashResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    fakeAccessor.keyStoreHashPattern = 0x5A;
    fakeAccessor.secureStoreHashPattern = 0xA5;
    InstallFakeAccessor(&fakeAccessor);

    OpenCDMSystem system("com.widevine.alpha", "meta");
    char version[64] = {0};
    uint64_t drmTime = 0;
    uint32_t ldlLimit = 0;
    uint8_t ids[8] = {0};
    uint32_t count = 0;
    uint8_t rawData[8] = {0};
    uint16_t rawSize = sizeof(rawData);
    uint8_t keyStoreHash[4] = {0};
    uint8_t secureStoreHash[4] = {0};
    const uint8_t sessionId[2] = {1, 2};
    const uint8_t response[2] = {3, 4};

    EXPECT_EQ(ERROR_NONE, opencdm_system_get_version(&system, version));
    EXPECT_STREQ("4.4.1", version);

    EXPECT_EQ(ERROR_NONE, opencdm_system_get_drm_time(&system, &drmTime));
    EXPECT_EQ(987654u, drmTime);

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_ldl_session_limit(&system, &ldlLimit));
    EXPECT_EQ(9u, ldlLimit);

    EXPECT_EQ(1u,
              opencdm_system_ext_is_secure_stop_enabled(&system));
    EXPECT_EQ(ERROR_NONE, opencdm_system_ext_enable_secure_stop(&system, 1));
    EXPECT_EQ(3u, opencdm_system_ext_reset_secure_stop(&system));

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_secure_stop_ids(&system, ids, sizeof(ids),
                                                     &count));
    EXPECT_EQ(2u, count);
    EXPECT_EQ(0x10, ids[0]);
    EXPECT_EQ(0x20, ids[1]);

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_secure_stop(&system, sessionId,
                                                 sizeof(sessionId), rawData,
                                                 &rawSize));
    EXPECT_EQ(3u, rawSize);
    EXPECT_EQ(0xAA, rawData[0]);
    EXPECT_EQ(0xBB, rawData[1]);
    EXPECT_EQ(0xCC, rawData[2]);

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_commit_secure_stop(&system, sessionId,
                                                    sizeof(sessionId), response,
                                                    sizeof(response)));
    EXPECT_EQ(ERROR_NONE, opencdm_delete_key_store(&system));
    EXPECT_EQ(ERROR_NONE, opencdm_delete_secure_store(&system));
    EXPECT_EQ(ERROR_NONE,
              opencdm_get_key_store_hash_ext(&system, keyStoreHash,
                                             sizeof(keyStoreHash)));
    EXPECT_EQ(ERROR_NONE,
              opencdm_get_secure_store_hash_ext(&system, secureStoreHash,
                                                sizeof(secureStoreHash)));
    EXPECT_EQ(0x5A, keyStoreHash[0]);
    EXPECT_EQ(0xA5, secureStoreHash[0]);

    UninstallFakeAccessor();
}

TEST(ClientOpenCdmExtL1Tests, CreateSystemExtendedWithoutAccessorReturnsInvalidAccessor)
{
    OpenCDMSystem* system = nullptr;

    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_create_system_extended("com.widevine.alpha", &system));
    EXPECT_EQ(nullptr, system);
}

TEST(ClientOpenCdmExtL1Tests, SystemGetVersionWithoutAccessorReturnsInvalidAccessor)
{
    OpenCDMSystem system("com.widevine.alpha", "meta");
    char version[32] = {0};

    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_get_version(&system, version));
}

TEST(ClientOpenCdmExtL1Tests, SystemGetDrmTimeWithoutAccessorReturnsInvalidAccessor)
{
    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint64_t drmTime = 0;

    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_get_drm_time(&system, &drmTime));
}

TEST(ClientOpenCdmExtL1Tests, SystemGetDrmTimeRejectsNullSystem)
{
    ScopedFakeAccessor scoped;
    uint64_t drmTime = 0;

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_get_drm_time(nullptr, &drmTime));
}

TEST(ClientOpenCdmExtL1Tests, SystemGetDrmTimeRejectsNullOutput)
{
    ScopedFakeAccessor scoped;
    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_get_drm_time(&system, nullptr));
}

TEST(ClientOpenCdmExtL1Tests, LdlSessionLimitRejectsNullSystem)
{
    ScopedFakeAccessor scoped;
    uint32_t ldlLimit = 0;

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_ext_get_ldl_session_limit(nullptr, &ldlLimit));
}

TEST(ClientOpenCdmExtL1Tests, LdlSessionLimitRejectsNullOutput)
{
    ScopedFakeAccessor scoped;
    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_ext_get_ldl_session_limit(&system, nullptr));
}

TEST(ClientOpenCdmExtL1Tests, SecureStopEnabledRejectsNullSystem)
{
    ScopedFakeAccessor scoped;

    EXPECT_EQ(static_cast<uint32_t>(ERROR_INVALID_ARG),
              opencdm_system_ext_is_secure_stop_enabled(nullptr));
}

TEST(ClientOpenCdmExtL1Tests, EnableSecureStopRejectsNullSystem)
{
    ScopedFakeAccessor scoped;

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_ext_enable_secure_stop(nullptr, 1));
}

TEST(ClientOpenCdmExtL1Tests, ResetSecureStopRejectsNullSystem)
{
    ScopedFakeAccessor scoped;

    EXPECT_EQ(static_cast<uint32_t>(ERROR_INVALID_ARG),
              opencdm_system_ext_reset_secure_stop(nullptr));
}

TEST(ClientOpenCdmExtL1Tests, GetSecureStopIdsRejectsNullSystem)
{
    ScopedFakeAccessor scoped;
    uint8_t ids[8] = {0};
    uint32_t count = 0;

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_ext_get_secure_stop_ids(nullptr, ids,
                                                     sizeof(ids), &count));
}

TEST(ClientOpenCdmExtL1Tests, GetSecureStopRejectsNullSystem)
{
    ScopedFakeAccessor scoped;
    const uint8_t sessionId[2] = {0x10, 0x11};
    uint8_t rawData[8] = {0};
    uint16_t rawSize = sizeof(rawData);

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_ext_get_secure_stop(nullptr, sessionId,
                                                 sizeof(sessionId), rawData,
                                                 &rawSize));
}

TEST(ClientOpenCdmExtL1Tests, CommitSecureStopRejectsNullSystem)
{
    ScopedFakeAccessor scoped;
    const uint8_t sessionId[2] = {0x10, 0x11};
    const uint8_t response[2] = {0x22, 0x33};

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_system_ext_commit_secure_stop(nullptr, sessionId,
                                                    sizeof(sessionId), response,
                                                    sizeof(response)));
}

TEST(ClientOpenCdmExtL1Tests, DeleteKeyStoreRejectsNullSystem)
{
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_delete_key_store(nullptr));
}

TEST(ClientOpenCdmExtL1Tests, DeleteSecureStoreRejectsNullSystem)
{
    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_delete_secure_store(nullptr));
}

TEST(ClientOpenCdmExtL1Tests, GetKeyStoreHashRejectsNullSystem)
{
    uint8_t keyHash[4] = {0};

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_get_key_store_hash_ext(nullptr, keyHash,
                                             sizeof(keyHash)));
}

TEST(ClientOpenCdmExtL1Tests, GetSecureStoreHashRejectsNullSystem)
{
    uint8_t secureHash[4] = {0};

    EXPECT_EQ(ERROR_INVALID_ARG,
              opencdm_get_secure_store_hash_ext(nullptr, secureHash,
                                                sizeof(secureHash)));
}

TEST(ClientOpenCdmExtL1Tests, GetPropertiesRejectsNullSystemPointer)
{
    const char* json = "{}";

    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_ext_get_properties(nullptr, json));
}

TEST(ClientOpenCdmExtL1Tests, GetPropertiesRejectsNullJsonPointer)
{
    PlayLevels levels {};

    EXPECT_EQ(ERROR_INVALID_ACCESSOR,
              opencdm_system_ext_get_properties(&levels, nullptr));
}

TEST(ClientOpenCdmExtL1Tests, SessionGetChallengeDataRejectsNullSessionAndSize)
{
    EXPECT_EQ(ERROR_INVALID_SESSION,
              opencdm_session_get_challenge_data(nullptr, nullptr,
                                                 nullptr, 0));
}

TEST(ClientOpenCdmExtL1Tests, ParseKeySystemWithOnlyOriginTokenReturnsBase)
{
    std::string keySystem("com.widevine.alpha;origin=");

    const OpenCDMError result = opencdm_parse_keysystem(keySystem);

    EXPECT_EQ(ERROR_NONE, result);
    EXPECT_EQ("com.widevine.alpha", keySystem);
}

TEST(ClientOpenCdmExtL1Tests, ParseKeySystemWithEmptyInputIsNoOp)
{
    std::string keySystem;

    const OpenCDMError result = opencdm_parse_keysystem(keySystem);

    EXPECT_EQ(ERROR_NONE, result);
    EXPECT_TRUE(keySystem.empty());
}

TEST(ClientOpenCdmExtL1Tests, ParseKeySystemWithUnrelatedDelimiterIsNoOp)
{
    std::string keySystem("com.widevine.alpha;foo=bar");

    const OpenCDMError result = opencdm_parse_keysystem(keySystem);

    EXPECT_EQ(ERROR_NONE, result);
    EXPECT_EQ("com.widevine.alpha;foo=bar", keySystem);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorVersionValueIsReturned)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.versionValue = "9.9.9";

    OpenCDMSystem system("com.widevine.alpha", "meta");
    char version[32] = {0};

    EXPECT_EQ(ERROR_NONE, opencdm_system_get_version(&system, version));
    EXPECT_STREQ("9.9.9", version);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorVersionCanBeEmptyString)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.versionValue.clear();

    OpenCDMSystem system("com.widevine.alpha", "meta");
    char version[32] = {0x7F};

    EXPECT_EQ(ERROR_NONE, opencdm_system_get_version(&system, version));
    EXPECT_STREQ("", version);
}

TEST(ClientOpenCdmExtL1Tests, GetVersionExtReturnsAccessorValue)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.versionValue = "7.3.1";

    OpenCDMSystem system("com.widevine.alpha", "meta");
    char version[32] = {0};

    EXPECT_EQ(ERROR_NONE, opencdm_system_get_version(&system, version));
    EXPECT_STREQ("7.3.1", version);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorDrmTimeZeroValueIsReturned)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.drmSystemTime = 0;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint64_t drmTime = 123;

    EXPECT_EQ(ERROR_NONE, opencdm_system_get_drm_time(&system, &drmTime));
    EXPECT_EQ(0u, drmTime);
}

TEST(ClientOpenCdmExtL1Tests, GetDrmSystemTimeReturnsAccessorValue)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.drmSystemTime = 987654321ull;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint64_t drmTime = 0;

    EXPECT_EQ(ERROR_NONE, opencdm_system_get_drm_time(&system, &drmTime));
    EXPECT_EQ(987654321ull, drmTime);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorLdlSessionLimitHighValueIsReturned)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.ldlSessionLimit = 255;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint32_t ldlLimit = 0;

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_ldl_session_limit(&system, &ldlLimit));
    EXPECT_EQ(255u, ldlLimit);
}

TEST(ClientOpenCdmExtL1Tests, GetLdlSessionLimitReturnsAccessorValue)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.ldlSessionLimit = 12;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint32_t ldlLimit = 0;

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_ldl_session_limit(&system, &ldlLimit));
    EXPECT_EQ(12u, ldlLimit);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorSecureStopEnabledFalseIsReturned)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStopEnabled = false;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(0u, opencdm_system_ext_is_secure_stop_enabled(&system));
}

TEST(ClientOpenCdmExtL1Tests, IsSecureStopEnabledReturnsAccessorValue)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStopEnabled = true;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(1u, opencdm_system_ext_is_secure_stop_enabled(&system));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorEnableSecureStopPropagatesFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.enableSecureStopResult = Exchange::OCDM_RESULT::OCDM_S_FALSE;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_S_FALSE),
              opencdm_system_ext_enable_secure_stop(&system, 1));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorResetSecureStopsCustomCountIsReturned)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.resetSecureStopsValue = 42;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(42u, opencdm_system_ext_reset_secure_stop(&system));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStopIdsTruncatesToProvidedBuffer)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStopCount = 4;
    scoped.accessor.secureStopIdsData = {0x01, 0x02, 0x03, 0x04, 0x05};

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint8_t ids[2] = {0};
    uint32_t count = 0;

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_secure_stop_ids(&system, ids,
                                                     sizeof(ids), &count));
    EXPECT_EQ(4u, count);
    EXPECT_EQ(0x01, ids[0]);
    EXPECT_EQ(0x02, ids[1]);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStopIdsUpdatesCountWithNullBuffer)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStopCount = 7;
    scoped.accessor.secureStopIdsData = {0x11, 0x22, 0x33};

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint32_t count = 0;

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_secure_stop_ids(&system, nullptr, 0,
                                                     &count));
    EXPECT_EQ(7u, count);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStopCopiesRawDataAndSize)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStopRawData = {0xCA, 0xCB, 0xCC, 0xCD};
    scoped.accessor.secureStopRawSize = 4;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t sessionId[2] = {0x10, 0x20};
    uint8_t rawData[8] = {0};
    uint16_t rawSize = sizeof(rawData);

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_secure_stop(&system, sessionId,
                                                 sizeof(sessionId), rawData,
                                                 &rawSize));
    EXPECT_EQ(4u, rawSize);
    EXPECT_EQ(0xCA, rawData[0]);
    EXPECT_EQ(0xCB, rawData[1]);
    EXPECT_EQ(0xCC, rawData[2]);
    EXPECT_EQ(0xCD, rawData[3]);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStopTruncatesToShortBuffer)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStopRawData = {0xD1, 0xD2, 0xD3};
    scoped.accessor.secureStopRawSize = 3;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t sessionId[1] = {0x01};
    uint8_t rawData[1] = {0};
    uint16_t rawSize = sizeof(rawData);

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_secure_stop(&system, sessionId,
                                                 sizeof(sessionId), rawData,
                                                 &rawSize));
    EXPECT_EQ(3u, rawSize);
    EXPECT_EQ(0xD1, rawData[0]);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStopSupportsNullRawBuffer)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStopRawData = {0xA1, 0xA2};
    scoped.accessor.secureStopRawSize = 2;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t sessionId[1] = {0x55};
    uint16_t rawSize = 99;

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_get_secure_stop(&system, sessionId,
                                                 sizeof(sessionId), nullptr,
                                                 &rawSize));
    EXPECT_EQ(2u, rawSize);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorCommitSecureStopPropagatesFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.commitSecureStopResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t sessionId[2] = {0x01, 0x02};
    const uint8_t response[2] = {0x03, 0x04};

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_system_ext_commit_secure_stop(&system, sessionId,
                                                    sizeof(sessionId), response,
                                                    sizeof(response)));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorCommitSecureStopAllowsNullPayloadWhenLengthZero)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.commitSecureStopResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_commit_secure_stop(&system, nullptr, 0,
                                                    nullptr, 0));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorDeleteKeyStorePropagatesFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.deleteKeyStoreResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_delete_key_store(&system));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorDeleteSecureStorePropagatesFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.deleteSecureStoreResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_delete_secure_store(&system));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorKeyStoreHashPatternIsApplied)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.keyStoreHashPattern = 0x6C;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint8_t hash[5] = {0};

    EXPECT_EQ(ERROR_NONE,
              opencdm_get_key_store_hash_ext(&system, hash, sizeof(hash)));
    EXPECT_EQ(0x6C, hash[0]);
    EXPECT_EQ(0x6C, hash[4]);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorSecureStoreHashPatternIsApplied)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStoreHashPattern = 0x39;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint8_t hash[6] = {0};

    EXPECT_EQ(ERROR_NONE,
              opencdm_get_secure_store_hash_ext(&system, hash, sizeof(hash)));
    EXPECT_EQ(0x39, hash[0]);
    EXPECT_EQ(0x39, hash[5]);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetKeyStoreHashPropagatesFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.keyStoreHashResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint8_t hash[2] = {0};

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_get_key_store_hash_ext(&system, hash, sizeof(hash)));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetKeyStoreHashAcceptsNullBufferWhenLengthZero)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.keyStoreHashResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(ERROR_NONE,
              opencdm_get_key_store_hash_ext(&system, nullptr, 0));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStoreHashPropagatesFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStoreHashResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint8_t hash[2] = {0};

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_get_secure_store_hash_ext(&system, hash, sizeof(hash)));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStoreHashAcceptsNullBufferWhenLengthZero)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStoreHashResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(ERROR_NONE,
              opencdm_get_secure_store_hash_ext(&system, nullptr, 0));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorCreateSystemExtendedUsesReturnedMetadata)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.metadataValue = "dynamic-meta-value";

    OpenCDMSystem* system = nullptr;

    ASSERT_EQ(ERROR_NONE,
              opencdm_create_system_extended("com.widevine.alpha", &system));
    ASSERT_NE(nullptr, system);
    EXPECT_EQ("dynamic-meta-value", system->Metadata());

    delete system;
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorCreateSystemExtendedTracksParsedKeySystem)
{
    ScopedFakeAccessor scoped;

    OpenCDMSystem* system = nullptr;

    ASSERT_EQ(ERROR_NONE,
              opencdm_create_system_extended("com.widevine.alpha;origin=site.invalid",
                                             &system));
    ASSERT_NE(nullptr, system);
    EXPECT_EQ("com.widevine.alpha", scoped.accessor.lastMetadataKeySystem);

    delete system;
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorCreateSystemExtendedFailureKeepsSystemNull)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.metadataResult = Exchange::OCDM_RESULT::OCDM_S_FALSE;

    OpenCDMSystem* system = reinterpret_cast<OpenCDMSystem*>(0x1);

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_S_FALSE),
              opencdm_create_system_extended("com.widevine.alpha", &system));
    EXPECT_EQ(nullptr, system);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorCreateSystemLegacyReturnsNullOnMetadataFailure)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.metadataResult = Exchange::OCDM_RESULT::OCDM_S_FALSE;

    OpenCDMSystem* system = opencdm_create_system("com.widevine.alpha");

    EXPECT_EQ(nullptr, system);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorCreateSystemLegacyReturnsSystemOnSuccess)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.metadataValue = "legacy-meta-ok";

    OpenCDMSystem* system = opencdm_create_system("com.widevine.alpha");

    ASSERT_NE(nullptr, system);
    EXPECT_EQ("legacy-meta-ok", system->Metadata());

    delete system;
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorVersionWriteOverwritesPreviousBufferContent)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.versionValue = "1";

    OpenCDMSystem system("com.widevine.alpha", "meta");
    char version[8] = {'X', 'X', 'X', 'X', 'X', 'X', 'X', '\0'};

    EXPECT_EQ(ERROR_NONE, opencdm_system_get_version(&system, version));
    EXPECT_STREQ("1", version);
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStopIdsPropagatesFailureCode)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.secureStopIdsResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    uint8_t ids[4] = {0};
    uint32_t count = 0;

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_system_ext_get_secure_stop_ids(&system, ids,
                                                     sizeof(ids), &count));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorGetSecureStopPropagatesFailureCode)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.getSecureStopResult = Exchange::OCDM_RESULT::OCDM_FAIL;

    OpenCDMSystem system("com.widevine.alpha", "meta");
    const uint8_t sessionId[1] = {0x01};
    uint8_t rawData[2] = {0};
    uint16_t rawSize = sizeof(rawData);

    EXPECT_EQ(static_cast<OpenCDMError>(Exchange::OCDM_RESULT::OCDM_FAIL),
              opencdm_system_ext_get_secure_stop(&system, sessionId,
                                                 sizeof(sessionId), rawData,
                                                 &rawSize));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorEnableSecureStopAcceptsZeroUseFlag)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.enableSecureStopResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;

    OpenCDMSystem system("com.widevine.alpha", "meta");

    EXPECT_EQ(ERROR_NONE,
              opencdm_system_ext_enable_secure_stop(&system, 0));
}

TEST(ClientOpenCdmExtL1Tests, MockAccessorCreateSystemExtendedSupportsEmptyMetadata)
{
    ScopedFakeAccessor scoped;
    scoped.accessor.metadataValue.clear();

    OpenCDMSystem* system = nullptr;

    ASSERT_EQ(ERROR_NONE,
              opencdm_create_system_extended("com.widevine.alpha", &system));
    ASSERT_NE(nullptr, system);
    EXPECT_EQ("", system->Metadata());

    delete system;
}

} // namespace
