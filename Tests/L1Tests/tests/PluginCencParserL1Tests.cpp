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

#include <array>
#include <cstdint>

#include "CENCParser.h"

using WPEFramework::Exchange::ISession;
using WPEFramework::Plugin::CommonEncryptionData;

namespace {

TEST(PluginCencParserL1Tests, EmptyInputStartsWithNoKeysAndPendingStatus)
{
    CommonEncryptionData parser(nullptr, 0);

    EXPECT_TRUE(parser.IsEmpty());
    EXPECT_EQ(ISession::StatusPending, parser.Status());
}

TEST(PluginCencParserL1Tests, AddKeyIdMakesKeyDiscoverable)
{
    CommonEncryptionData parser(nullptr, 0);

    const std::array<uint8_t, 16> key = {
        0x01, 0x23, 0x45, 0x67,
        0x89, 0xab, 0xcd, 0xef,
        0x10, 0x32, 0x54, 0x76,
        0x98, 0xba, 0xdc, 0xfe
    };

    CommonEncryptionData::KeyId keyId(CommonEncryptionData::WIDEVINE,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));

    parser.AddKeyId(keyId);

    EXPECT_FALSE(parser.IsEmpty());
    EXPECT_TRUE(parser.HasKeyId(keyId));
}

TEST(PluginCencParserL1Tests, UpdateKeyStatusReflectsUsableState)
{
    CommonEncryptionData parser(nullptr, 0);

    const std::array<uint8_t, 16> key = {
        0xaa, 0xbb, 0xcc, 0xdd,
        0xee, 0xff, 0x00, 0x11,
        0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99
    };

    CommonEncryptionData::KeyId keyId(CommonEncryptionData::PLAYREADY,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));

    const CommonEncryptionData::KeyId* updated =
        parser.UpdateKeyStatus(ISession::Usable, keyId);

    ASSERT_NE(nullptr, updated);
    EXPECT_EQ(ISession::Usable, updated->Status());
    EXPECT_EQ(ISession::Usable, parser.Status());
}

TEST(PluginCencParserL1Tests, StatusForUnknownKeyIsPending)
{
    CommonEncryptionData parser(nullptr, 0);

    const std::array<uint8_t, 16> key = {
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
        0x99, 0xaa, 0xbb, 0xcc,
        0xdd, 0xee, 0xff, 0x00
    };

    CommonEncryptionData::KeyId keyId(CommonEncryptionData::COMMON,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));

    EXPECT_EQ(ISession::StatusPending, parser.Status(keyId));
}

} // namespace
