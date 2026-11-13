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

#include <rdk_perf.h>

#include "open_cdm.h"
#include "open_cdm_ext.h"
#include "open_cdm_impl.h"
#include "FakeServerInterfaces.h"
#include "ocdm_perf.h"

std::vector<RDKPerfStubRecord>& RDKPerfStubRecords()
{
    static std::vector<RDKPerfStubRecord> records;
    return records;
}

namespace {

class ClientOpenCdmPerfL1Tests : public ::testing::Test {
protected:
    void SetUp() override
    {
        InstallFakeAccessor(&_accessor);
        RDKPerfStubRecords().clear();
    }

    void TearDown() override
    {
        UninstallFakeAccessor();
        RDKPerfStubRecords().clear();
    }

    static std::vector<std::string> RecordedNames()
    {
        std::vector<std::string> names;
        for (const auto& record : RDKPerfStubRecords()) {
            names.push_back(record.name);
        }
        return names;
    }

    // Each instrumented call must open exactly one scope named after the
    // function and close it when the function returns.
    static void ExpectSingleClosedScope(const std::string& name)
    {
        ASSERT_EQ(std::vector<std::string>({ name }), RecordedNames());
        EXPECT_FALSE(RDKPerfStubRecords().front().open);
        RDKPerfStubRecords().clear();
    }

    FakeOpenCDMAccessor _accessor;
};

TEST_F(ClientOpenCdmPerfL1Tests, PerfScopeStaysOpenUntilEndOfScope)
{
    {
        OCDM_PERF_SCOPE("scope_under_test");
        ASSERT_EQ(std::vector<std::string>({ "scope_under_test" }), RecordedNames());
        EXPECT_TRUE(RDKPerfStubRecords().front().open);
    }
    EXPECT_FALSE(RDKPerfStubRecords().front().open);
}

TEST_F(ClientOpenCdmPerfL1Tests, SystemApisAreInstrumented)
{
    OpenCDMSystem* system = nullptr;
    _accessor.metadataResult = Exchange::OCDM_RESULT::OCDM_SUCCESS;
    EXPECT_EQ(ERROR_NONE, opencdm_create_system_extended("com.widevine.alpha", &system));
    ExpectSingleClosedScope("opencdm_create_system_extended");
    ASSERT_NE(nullptr, system);

    _accessor.isTypeSupportedValue = true;
    EXPECT_EQ(ERROR_NONE, opencdm_is_type_supported("com.widevine.alpha", "video/mp4"));
    ExpectSingleClosedScope("opencdm_is_type_supported");

    const uint8_t certificate[] = { 0x01, 0x02 };
    opencdm_system_set_server_certificate(system, certificate, sizeof(certificate));
    ExpectSingleClosedScope("opencdm_system_set_server_certificate");

    const uint8_t keyId[] = { 0x01, 0x02 };
    EXPECT_EQ(nullptr, opencdm_get_system_session(system, keyId, sizeof(keyId), 0));
    ExpectSingleClosedScope("opencdm_get_system_session");

    EXPECT_EQ(ERROR_NONE, opencdm_destruct_system(system));
    ExpectSingleClosedScope("opencdm_destruct_system");
}

TEST_F(ClientOpenCdmPerfL1Tests, DeprecatedCreateSystemIsTimedOnceThroughExtended)
{
    OpenCDMSystem* system = opencdm_create_system("com.widevine.alpha");
    ExpectSingleClosedScope("opencdm_create_system_extended");
    if (system != nullptr) {
        opencdm_destruct_system(system);
    }
}

TEST_F(ClientOpenCdmPerfL1Tests, SessionLifecycleApisAreInstrumented)
{
    OpenCDMSession* session = nullptr;
    opencdm_construct_session(nullptr, Temporary, "cenc", nullptr, 0, nullptr, 0,
                              nullptr, nullptr, &session);
    ExpectSingleClosedScope("opencdm_construct_session");

    uint8_t challenge[8] = {};
    uint32_t challengeSize = sizeof(challenge);
    opencdm_session_get_challenge_data(nullptr, challenge, &challengeSize, 0);
    ExpectSingleClosedScope("opencdm_session_get_challenge_data");

    const uint8_t message[] = { 0x01 };
    opencdm_session_update(nullptr, message, sizeof(message));
    ExpectSingleClosedScope("opencdm_session_update");

    opencdm_session_load(nullptr);
    ExpectSingleClosedScope("opencdm_session_load");

    uint8_t secureStopId[16] = {};
    opencdm_session_store_license_data(nullptr, message, sizeof(message), secureStopId);
    ExpectSingleClosedScope("opencdm_session_store_license_data");

    opencdm_session_remove(nullptr);
    ExpectSingleClosedScope("opencdm_session_remove");

    opencdm_session_close(nullptr);
    ExpectSingleClosedScope("opencdm_session_close");

    opencdm_destruct_session(nullptr);
    ExpectSingleClosedScope("opencdm_destruct_session");
}

TEST_F(ClientOpenCdmPerfL1Tests, DecryptApisAreInstrumented)
{
    uint8_t encrypted[4] = {};
    const uint8_t iv[16] = {};
    const uint8_t keyId[16] = {};
    EncryptionPattern pattern = { 0, 0 };

    opencdm_session_decrypt(nullptr, encrypted, sizeof(encrypted),
                            EncryptionScheme::AesCtr_Cenc, pattern,
                            iv, sizeof(iv), keyId, sizeof(keyId), 0);
    ExpectSingleClosedScope("opencdm_session_decrypt");

    opencdm_session_decrypt_v2(nullptr, encrypted, sizeof(encrypted), nullptr, nullptr);
    ExpectSingleClosedScope("opencdm_session_decrypt_v2");

    opencdm_session_decrypt_v3(nullptr, encrypted, sizeof(encrypted), nullptr, 0, nullptr);
    ExpectSingleClosedScope("opencdm_session_decrypt_v3");

    opencdm_session_decrypt_fps(nullptr, 1, 2, 3, 0xA, encrypted, sizeof(encrypted),
                                nullptr, 0, nullptr, nullptr);
    ExpectSingleClosedScope("opencdm_session_decrypt_fps");
}

TEST_F(ClientOpenCdmPerfL1Tests, TrivialAccessorsAreNotInstrumented)
{
    const uint8_t keyId[] = { 0x01 };
    opencdm_session_id(nullptr);
    opencdm_session_buffer_id(nullptr);
    opencdm_session_status(nullptr, keyId, sizeof(keyId));
    opencdm_session_has_key_id(nullptr, sizeof(keyId), keyId);
    opencdm_session_error(nullptr, keyId, sizeof(keyId));

    EXPECT_TRUE(RDKPerfStubRecords().empty());
}

} // namespace
