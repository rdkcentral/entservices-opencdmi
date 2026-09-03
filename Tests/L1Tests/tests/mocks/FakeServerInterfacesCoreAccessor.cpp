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

#include "FakeServerInterfaces.h"

template <typename Tag, typename Tag::type Member>
struct PrivateAccess {
    friend typename Tag::type GetMember(Tag)
    {
        return Member;
    }
};

struct SingletonMemberTag {
    using type = OpenCDMAccessor**;
    friend type GetMember(SingletonMemberTag);
};

template struct PrivateAccess<
    SingletonMemberTag,
    &Core::SingletonType<OpenCDMAccessor>::g_TypedSingleton>;

namespace {
OpenCDMAccessor* g_previousAccessor = nullptr;
}

void InstallFakeAccessor(OpenCDMAccessor* accessor)
{
    ASSERT(accessor != nullptr);
    ASSERT(g_previousAccessor == nullptr);

    OpenCDMAccessor::Instance();
    auto singletonMember = GetMember(SingletonMemberTag {});
    g_previousAccessor = *singletonMember;
    *singletonMember = accessor;
}

void UninstallFakeAccessor()
{
    ASSERT(g_previousAccessor != nullptr);

    auto singletonMember = GetMember(SingletonMemberTag {});
    *singletonMember = g_previousAccessor;
    g_previousAccessor = nullptr;
}
