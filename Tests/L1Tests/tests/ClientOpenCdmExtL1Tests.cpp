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

#include "open_cdm_impl.h"
#include "open_cdm.h"
#include "open_cdm_ext.h"

OpenCDMError opencdm_parse_keysystem(std::string& keySystemDomain);

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

} // namespace
