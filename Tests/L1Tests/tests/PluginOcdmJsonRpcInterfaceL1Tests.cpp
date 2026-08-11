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

#include <atomic>

#define private public
#include "OCDM.h"
#undef private

#include "FakeServerInterfaces.h"

using WPEFramework::Core::JSON::ArrayType;
using WPEFramework::Core::JSON::String;
using WPEFramework::JsonData::OCDM::DrmData;
using WPEFramework::Plugin::FakeContentDecryption;
using WPEFramework::Plugin::OCDM;

namespace {

class TestOCDM : public OCDM {
public:
    uint32_t AddRef() const override
    {
        return ++_refCount;
    }

    uint32_t Release() const override
    {
        const uint32_t value = --_refCount;
        if (value == 0) {
            delete this;
            return WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
        }

        return WPEFramework::Core::ERROR_NONE;
    }

private:
    mutable std::atomic<uint32_t> _refCount{1};
};

TEST(PluginOcdmJsonRpcInterfaceL1Tests, GetDrmsReturnsSystemsAndDesignators)
{
    TestOCDM plugin;
    auto* fake = new FakeContentDecryption();
    fake->SetSystems({"widevine", "playready"});
    fake->SetDesignators("widevine", {"com.widevine.alpha"});
    fake->SetDesignators("playready", {"com.microsoft.playready"});

    plugin._opencdmi = fake;

    ArrayType<DrmData> response;
    const uint32_t rc = plugin.get_drms(response);

    EXPECT_EQ(WPEFramework::Core::ERROR_NONE, rc);
    EXPECT_EQ(2u, response.Length());

    auto it = response.Elements();
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("widevine", it.Current().Name.Value());
    ASSERT_TRUE(it.Current().Keysystems.Elements().Next());

    fake->Release();
    plugin._opencdmi = nullptr;
}

TEST(PluginOcdmJsonRpcInterfaceL1Tests, GetDrmsReturnsEmptyListWhenNoSystemsAvailable)
{
    TestOCDM plugin;
    auto* fake = new FakeContentDecryption();

    plugin._opencdmi = fake;

    ArrayType<DrmData> response;
    const uint32_t rc = plugin.get_drms(response);

    EXPECT_EQ(WPEFramework::Core::ERROR_NONE, rc);
    EXPECT_EQ(0u, response.Length());

    fake->Release();
    plugin._opencdmi = nullptr;
}

TEST(PluginOcdmJsonRpcInterfaceL1Tests, GetDrmsKeepsSystemWhenDesignatorsUnavailable)
{
    TestOCDM plugin;
    auto* fake = new FakeContentDecryption();
    fake->SetSystems({"widevine"});

    plugin._opencdmi = fake;

    ArrayType<DrmData> response;
    const uint32_t rc = plugin.get_drms(response);

    EXPECT_EQ(WPEFramework::Core::ERROR_NONE, rc);
    ASSERT_EQ(1u, response.Length());

    auto it = response.Elements();
    ASSERT_TRUE(it.Next());
    EXPECT_EQ("widevine", it.Current().Name.Value());
    EXPECT_EQ(0u, it.Current().Keysystems.Length());

    fake->Release();
    plugin._opencdmi = nullptr;
}

TEST(PluginOcdmJsonRpcInterfaceL1Tests, GetKeysystemsForKnownSystemReturnsList)
{
    TestOCDM plugin;
    auto* fake = new FakeContentDecryption();
    fake->SetDesignators("widevine", {"com.widevine.alpha", "com.widevine.test"});

    plugin._opencdmi = fake;

    ArrayType<String> response;
    const uint32_t rc = plugin.get_keysystems("widevine", response);

    EXPECT_EQ(WPEFramework::Core::ERROR_NONE, rc);
    EXPECT_EQ(2u, response.Length());

    fake->Release();
    plugin._opencdmi = nullptr;
}

TEST(PluginOcdmJsonRpcInterfaceL1Tests, GetKeysystemsForUnknownSystemReturnsBadRequest)
{
    TestOCDM plugin;
    auto* fake = new FakeContentDecryption();

    plugin._opencdmi = fake;

    ArrayType<String> response;
    const uint32_t rc = plugin.get_keysystems("unknown", response);

    EXPECT_EQ(WPEFramework::Core::ERROR_BAD_REQUEST, rc);
    EXPECT_EQ(0u, response.Length());

    fake->Release();
    plugin._opencdmi = nullptr;
}

TEST(PluginOcdmJsonRpcInterfaceL1Tests, GetKeysystemsForKnownSystemCanReturnEmptyList)
{
    TestOCDM plugin;
    auto* fake = new FakeContentDecryption();
    fake->SetDesignators("playready", {});

    plugin._opencdmi = fake;

    ArrayType<String> response;
    const uint32_t rc = plugin.get_keysystems("playready", response);

    EXPECT_EQ(WPEFramework::Core::ERROR_NONE, rc);
    EXPECT_EQ(0u, response.Length());

    fake->Release();
    plugin._opencdmi = nullptr;
}

} // namespace
