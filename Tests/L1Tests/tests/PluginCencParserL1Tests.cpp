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

TEST(PluginCencParserL1Tests, CopyConstructorPreservesKeyIds)
{
    CommonEncryptionData original(nullptr, 0);

    const std::array<uint8_t, 16> key = {
        0xAA, 0xBB, 0xCC, 0xDD,
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C
    };
    CommonEncryptionData::KeyId keyId(CommonEncryptionData::COMMON,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));
    original.AddKeyId(keyId);

    CommonEncryptionData copy(original);

    EXPECT_FALSE(copy.IsEmpty());
    EXPECT_TRUE(copy.HasKeyId(keyId));
}

TEST(PluginCencParserL1Tests, KeyIdFlagAccumulatesSystemsOnDuplicateAdd)
{
    CommonEncryptionData parser(nullptr, 0);

    const std::array<uint8_t, 16> key = {
        0x10, 0x20, 0x30, 0x40,
        0x50, 0x60, 0x70, 0x80,
        0x90, 0xA0, 0xB0, 0xC0,
        0xD0, 0xE0, 0xF0, 0x00
    };

    CommonEncryptionData::KeyId wvKey(CommonEncryptionData::WIDEVINE,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));
    CommonEncryptionData::KeyId prKey(CommonEncryptionData::PLAYREADY,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));

    parser.AddKeyId(wvKey);
    parser.AddKeyId(prKey);

    CommonEncryptionData::Iterator it = parser.Keys();
    ASSERT_TRUE(it.Next());
    EXPECT_EQ(static_cast<uint32_t>(CommonEncryptionData::WIDEVINE |
                                    CommonEncryptionData::PLAYREADY),
              it.Current().Systems());
    EXPECT_FALSE(it.Next());
}

TEST(PluginCencParserL1Tests, KeyIdSystemsReturnsInitialSystem)
{
    const std::array<uint8_t, 16> key = {
        0xF1, 0xF2, 0xF3, 0xF4,
        0xF5, 0xF6, 0xF7, 0xF8,
        0xF9, 0xFA, 0xFB, 0xFC,
        0xFD, 0xFE, 0xFF, 0x00
    };
    CommonEncryptionData::KeyId keyId(CommonEncryptionData::CLEARKEY,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));

    EXPECT_EQ(static_cast<uint32_t>(CommonEncryptionData::CLEARKEY),
              keyId.Systems());

    keyId.Flag(CommonEncryptionData::WIDEVINE);
    EXPECT_EQ(static_cast<uint32_t>(CommonEncryptionData::CLEARKEY |
                                    CommonEncryptionData::WIDEVINE),
              keyId.Systems());
}

TEST(PluginCencParserL1Tests, KeysIteratorReturnsAllAddedKeys)
{
    CommonEncryptionData parser(nullptr, 0);

    const std::array<uint8_t, 16> key1 = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    const std::array<uint8_t, 16> key2 = {
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
    };

    parser.AddKeyId(CommonEncryptionData::KeyId(CommonEncryptionData::WIDEVINE,
                                                key1.data(),
                                                static_cast<uint8_t>(key1.size())));
    parser.AddKeyId(CommonEncryptionData::KeyId(CommonEncryptionData::PLAYREADY,
                                                key2.data(),
                                                static_cast<uint8_t>(key2.size())));

    CommonEncryptionData::Iterator it = parser.Keys();
    uint32_t count = 0;
    while (it.Next()) {
        ++count;
    }
    EXPECT_EQ(2u, count);
}

TEST(PluginCencParserL1Tests, IsSupportedReturnsTrueWhenAllRequestedKeysPresent)
{
    CommonEncryptionData available(nullptr, 0);
    const std::array<uint8_t, 16> key = {
        0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
        0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0
    };
    CommonEncryptionData::KeyId keyId(CommonEncryptionData::WIDEVINE,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));
    available.AddKeyId(keyId);

    CommonEncryptionData requested(nullptr, 0);
    requested.AddKeyId(keyId);

    EXPECT_TRUE(available.IsSupported(requested));
}

TEST(PluginCencParserL1Tests, IsSupportedReturnsTrueForEmptyRequested)
{
    CommonEncryptionData available(nullptr, 0);
    CommonEncryptionData requested(nullptr, 0);
    EXPECT_TRUE(available.IsSupported(requested));
}

TEST(PluginCencParserL1Tests, IsSupportedReturnsFalseWhenKeyMissing)
{
    CommonEncryptionData available(nullptr, 0);
    CommonEncryptionData requested(nullptr, 0);

    const std::array<uint8_t, 16> key = {
        0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
        0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0
    };
    CommonEncryptionData::KeyId keyId(CommonEncryptionData::WIDEVINE,
                                      key.data(),
                                      static_cast<uint8_t>(key.size()));
    requested.AddKeyId(keyId);

    EXPECT_FALSE(available.IsSupported(requested));
}

TEST(PluginCencParserL1Tests, ParsePsshV1ClearKeyBoxAddsKeyId)
{
    // PSSH v1 box: 4-byte size | "pssh" | version=1 flags=0 |
    //              ClearKey system ID | key count=1 | key ID | pssh data size=0
    const std::array<uint8_t, 52> psshBox = {{
        0x00, 0x00, 0x00, 0x34,  // box size = 52
        0x70, 0x73, 0x73, 0x68,  // "pssh"
        0x01, 0x00, 0x00, 0x00,  // version=1, flags=0
        0x58, 0x14, 0x7e, 0xc8, 0x04, 0x23, 0x46, 0x59,  // ClearKey system ID
        0x92, 0xe6, 0xf5, 0x2c, 0x5c, 0xe8, 0xc3, 0xcc,
        0x00, 0x00, 0x00, 0x01,  // key count = 1
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,  // key ID
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x00, 0x00, 0x00, 0x00   // pssh data size = 0
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_FALSE(parser.IsEmpty());

    const std::array<uint8_t, 16> keyId = {{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
    }};
    CommonEncryptionData::KeyId expectedKey(CommonEncryptionData::CLEARKEY,
                                            keyId.data(),
                                            static_cast<uint8_t>(keyId.size()));
    EXPECT_TRUE(parser.HasKeyId(expectedKey));
}

TEST(PluginCencParserL1Tests, ParsePsshV0CommonEncryptionBoxProducesNoKeys)
{
    // PSSH v0 box with CommonEncryption system ID and no payload
    const std::array<uint8_t, 32> psshBox = {{
        0x00, 0x00, 0x00, 0x20,  // box size = 32
        0x70, 0x73, 0x73, 0x68,  // "pssh"
        0x00, 0x00, 0x00, 0x00,  // version=0, flags=0
        0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,  // CommonEncryption system ID
        0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b,
        0x00, 0x00, 0x00, 0x00   // pssh data size = 0
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_TRUE(parser.IsEmpty());
    EXPECT_EQ(ISession::StatusPending, parser.Status());
}

TEST(PluginCencParserL1Tests, ParsePsshV0UnknownSystemProducesNoKeys)
{
    // PSSH v0 box with an unrecognised system ID
    const std::array<uint8_t, 32> psshBox = {{
        0x00, 0x00, 0x00, 0x20,  // box size = 32
        0x70, 0x73, 0x73, 0x68,  // "pssh"
        0x00, 0x00, 0x00, 0x00,  // version=0, flags=0
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // unknown system ID
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x00, 0x00   // pssh data size = 0
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

TEST(PluginCencParserL1Tests, ParsePsshBoxTooSmallForHeaderProducesNoKeys)
{
    // Box that declares 16 bytes (only 8 bytes after "pssh"), too small for ParsePSSHBox
    const std::array<uint8_t, 16> psshBox = {{
        0x00, 0x00, 0x00, 0x10,  // box size = 16
        0x70, 0x73, 0x73, 0x68,  // "pssh"
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07  // 8 bytes (< 24 header minimum)
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

TEST(PluginCencParserL1Tests, ParsePlayReadyPsshV0WithMinimalPayloadProducesNoKeys)
{
    // PlayReady v0 PSSH with a minimal valid payload: size=6, count=0
    const std::array<uint8_t, 38> psshBox = {{
        0x00, 0x00, 0x00, 0x26,  // box size = 38
        0x70, 0x73, 0x73, 0x68,  // "pssh"
        0x00, 0x00, 0x00, 0x00,  // version=0, flags=0
        0x9a, 0x04, 0xf0, 0x79, 0x98, 0x40, 0x42, 0x86,  // PlayReady system ID
        0xab, 0x92, 0xe6, 0x5b, 0xe0, 0x88, 0x5f, 0x95,
        0x00, 0x00, 0x00, 0x06,  // pssh data size = 6
        0x06, 0x00, 0x00, 0x00,  // PR payload: total size=6 (LE)
        0x00, 0x00               // PR payload: record count=0
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

TEST(PluginCencParserL1Tests, ParseWidevinePsshV0WithGarbagePayloadProducesNoKeys)
{
    // Widevine v0 PSSH with 4 bytes of non-protobuf payload
    const std::array<uint8_t, 36> psshBox = {{
        0x00, 0x00, 0x00, 0x24,  // box size = 36
        0x70, 0x73, 0x73, 0x68,  // "pssh"
        0x00, 0x00, 0x00, 0x00,  // version=0, flags=0
        0xed, 0xef, 0x8b, 0xa9, 0x79, 0xd6, 0x4a, 0xce,  // Widevine system ID
        0xa3, 0xc8, 0x27, 0xdc, 0xd5, 0x1d, 0x21, 0xed,
        0x00, 0x00, 0x00, 0x04,  // pssh data size = 4
        0xFF, 0xFF, 0xFF, 0xFF   // invalid protobuf
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

TEST(PluginCencParserL1Tests, ParseJsonInitDataWithEmptyKidsProducesNoKeys)
{
    const std::string json = "{\"kids\":[]}"; // JSONKeyIds pattern with empty array

    CommonEncryptionData parser(
        reinterpret_cast<const uint8_t*>(json.c_str()),
        static_cast<uint16_t>(json.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

TEST(PluginCencParserL1Tests, ParseUnrecognisedDataProducesNoKeys)
{
    // Random bytes: no pssh magic, no JSON pattern, no PlayReady XML
    const std::array<uint8_t, 8> garbage = {{
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    }};

    CommonEncryptionData parser(garbage.data(),
                                static_cast<uint16_t>(garbage.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

TEST(PluginCencParserL1Tests, ParsePsshV1WithExcessiveKeyCountIsRejected)
{
    // v1 PSSH where key count (100) far exceeds the available buffer space
    const std::array<uint8_t, 52> psshBox = {{
        0x00, 0x00, 0x00, 0x34,
        0x70, 0x73, 0x73, 0x68,
        0x01, 0x00, 0x00, 0x00,
        0x58, 0x14, 0x7e, 0xc8, 0x04, 0x23, 0x46, 0x59,
        0x92, 0xe6, 0xf5, 0x2c, 0x5c, 0xe8, 0xc3, 0xcc,
        0x00, 0x00, 0x00, 0x64,  // key count = 100 (requires 1628 bytes, have 44)
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x00, 0x00, 0x00, 0x00
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

TEST(PluginCencParserL1Tests, ParsePlayReadyPsshWithMismatchedInternalSizeProducesNoKeys)
{
    // PlayReady PSSH where the embedded LE size (10) != psshDataSize (6)
    const std::array<uint8_t, 38> psshBox = {{
        0x00, 0x00, 0x00, 0x26,
        0x70, 0x73, 0x73, 0x68,
        0x00, 0x00, 0x00, 0x00,
        0x9a, 0x04, 0xf0, 0x79, 0x98, 0x40, 0x42, 0x86,
        0xab, 0x92, 0xe6, 0x5b, 0xe0, 0x88, 0x5f, 0x95,
        0x00, 0x00, 0x00, 0x06,  // psshDataSize = 6
        0x0A, 0x00, 0x00, 0x00,  // PR: Read32LE = 10, mismatches psshDataSize
        0x00, 0x00
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

TEST(PluginCencParserL1Tests, ParsePsshV0WithOversizedPayloadDeclarationIsRejected)
{
    // v0 PSSH where psshDataSize (100) + header (24) > actual length (24)
    const std::array<uint8_t, 32> psshBox = {{
        0x00, 0x00, 0x00, 0x20,
        0x70, 0x73, 0x73, 0x68,
        0x00, 0x00, 0x00, 0x00,
        0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02,
        0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b,
        0x00, 0x00, 0x00, 0x64   // psshDataSize = 100 (far exceeds buffer)
    }};

    CommonEncryptionData parser(psshBox.data(),
                                static_cast<uint16_t>(psshBox.size()));

    EXPECT_TRUE(parser.IsEmpty());
}

} // namespace
