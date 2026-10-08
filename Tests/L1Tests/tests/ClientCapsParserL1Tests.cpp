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

#include "CapsParser.h"

using WPEFramework::Plugin::CapsParser;

namespace {

TEST(ClientCapsParserL1Tests, ParseVideoCapsWithDecryptToHostTrue)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1920, height=(int)1080, decrypt-to-host=(boolean)true";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Video, parser.GetMediaType());
    EXPECT_EQ(1920, parser.GetWidth());
    EXPECT_EQ(1080, parser.GetHeight());
    EXPECT_TRUE(parser.IsSecureMemoryDisabled());
}

TEST(ClientCapsParserL1Tests, ParseVideoCapsWithDecryptToHostFalse)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1280, height=(int)720, decrypt-to-host=(boolean)false";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Video, parser.GetMediaType());
    EXPECT_EQ(1280, parser.GetWidth());
    EXPECT_EQ(720, parser.GetHeight());
    EXPECT_FALSE(parser.IsSecureMemoryDisabled());
}

TEST(ClientCapsParserL1Tests, ParseAudioCapsResetsVideoDimensions)
{
    CapsParser parser;
    const std::string caps =
        "audio/mpeg, original-media-type=(string)audio, channels=(int)2";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Audio, parser.GetMediaType());
    EXPECT_EQ(0, parser.GetWidth());
    EXPECT_EQ(0, parser.GetHeight());
    EXPECT_FALSE(parser.IsSecureMemoryDisabled());
}

TEST(ClientCapsParserL1Tests, ParseUnknownMediaLeavesUnknownType)
{
    CapsParser parser;
    const std::string caps =
        "application/x-custom, original-media-type=(string)subtitle";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Unknown, parser.GetMediaType());
    EXPECT_EQ(0, parser.GetWidth());
    EXPECT_EQ(0, parser.GetHeight());
}

TEST(ClientCapsParserL1Tests, ParseZeroLengthInputIsNoOp)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1920, height=(int)1080, decrypt-to-host=(boolean)false";
    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    const std::string audio = "audio/mpeg, original-media-type=(string)audio";
    parser.Parse(reinterpret_cast<const uint8_t*>(audio.c_str()), 0);

    EXPECT_EQ(CDMi::Video, parser.GetMediaType());
    EXPECT_EQ(1920, parser.GetWidth());
}

TEST(ClientCapsParserL1Tests, ParseSameCapsTwiceUsesCachedResult)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)640, height=(int)480, decrypt-to-host=(boolean)false";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));
    EXPECT_EQ(640, parser.GetWidth());

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));
    EXPECT_EQ(640, parser.GetWidth());
}

TEST(ClientCapsParserL1Tests, ParseVideoWithoutWidthAndHeightGivesZeroDimensions)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "decrypt-to-host=(boolean)false";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Video, parser.GetMediaType());
    EXPECT_EQ(0, parser.GetWidth());
    EXPECT_EQ(0, parser.GetHeight());
}

TEST(ClientCapsParserL1Tests, ParseAudioViaStrcasesstrFallbackWithoutMediaTypeTag)
{
    CapsParser parser;
    const std::string caps = "audio/mpeg, channels=(int)2";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Audio, parser.GetMediaType());
    EXPECT_EQ(0, parser.GetWidth());
    EXPECT_EQ(0, parser.GetHeight());
}

TEST(ClientCapsParserL1Tests, ParseVideoViaStrcasesstrFallbackWithoutMediaTypeTag)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, width=(int)1280, height=(int)720, "
        "decrypt-to-host=(boolean)false";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Video, parser.GetMediaType());
    EXPECT_EQ(1280, parser.GetWidth());
    EXPECT_EQ(720, parser.GetHeight());
}

TEST(ClientCapsParserL1Tests, ParseNoRecognizedMediaInFallbackKeepsUnknown)
{
    CapsParser parser;
    const std::string caps = "application/octet-stream, rate=(int)44100";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Unknown, parser.GetMediaType());
}

TEST(ClientCapsParserL1Tests, ParseVideoWithDecryptToHostNumericOneIsSecureMemoryDisabled)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1920, height=(int)1080, decrypt-to-host=(boolean)1";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_TRUE(parser.IsSecureMemoryDisabled());
}

TEST(ClientCapsParserL1Tests, ParseVideoWithDecryptToHostUppercaseTIsSecureMemoryDisabled)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1920, height=(int)1080, decrypt-to-host=(boolean)True";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_TRUE(parser.IsSecureMemoryDisabled());
}

TEST(ClientCapsParserL1Tests, ParseVideoWithDecryptToHostWithoutBooleanPrefixTrue)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1920, height=(int)1080, decrypt-to-host=true";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_TRUE(parser.IsSecureMemoryDisabled());
}

TEST(ClientCapsParserL1Tests, ParseVideoWithOnlyWidthMissingHeightSetsHeightZero)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1280, decrypt-to-host=(boolean)false";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_EQ(CDMi::Video, parser.GetMediaType());
    EXPECT_EQ(1280, parser.GetWidth());
    EXPECT_EQ(0, parser.GetHeight());
}

TEST(ClientCapsParserL1Tests, ParseVideoWithDecryptToHostNumericZeroIsNotSecureMemoryDisabled)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1920, height=(int)1080, decrypt-to-host=(boolean)0";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_FALSE(parser.IsSecureMemoryDisabled());
}

TEST(ClientCapsParserL1Tests, ParseVideoWithDecryptToHostUppercaseFalseIsNotSecureMemoryDisabled)
{
    CapsParser parser;
    const std::string caps =
        "video/x-h264, original-media-type=(string)video, "
        "width=(int)1920, height=(int)1080, decrypt-to-host=(boolean)False";

    parser.Parse(reinterpret_cast<const uint8_t*>(caps.c_str()),
                 static_cast<uint16_t>(caps.size()));

    EXPECT_FALSE(parser.IsSecureMemoryDisabled());
}

} // namespace
