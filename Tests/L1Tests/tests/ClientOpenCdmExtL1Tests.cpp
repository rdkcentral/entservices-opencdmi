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

} // namespace
