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

#include <gtest/gtest.h>

template <typename Tag, typename Tag::type Member>
struct PrivateAccess {
    friend typename Tag::type GetMember(Tag)
    {
        return Member;
    }
};

struct RemoteMemberTag {
    using type = Exchange::IAccessorOCDM* OpenCDMAccessor::*;
    friend type GetMember(RemoteMemberTag);
};

template struct PrivateAccess<RemoteMemberTag, &OpenCDMAccessor::_remote>;

namespace {
Exchange::IAccessorOCDM* g_previousRemote = nullptr;
}

void InstallFakeAccessor(OpenCDMAccessor* accessor)
{
    OpenCDMAccessor* realAccessor = OpenCDMAccessor::Instance();
    ASSERT(realAccessor != nullptr);

    if (realAccessor != nullptr) {
        auto remoteMember = GetMember(RemoteMemberTag {});
        g_previousRemote = realAccessor->*remoteMember;
        realAccessor->*remoteMember = accessor;

    }
}

void UninstallFakeAccessor()
{
    OpenCDMAccessor* realAccessor = OpenCDMAccessor::Instance();
    ASSERT(realAccessor != nullptr);

    if (realAccessor != nullptr) {
        auto remoteMember = GetMember(RemoteMemberTag {});
        realAccessor->*remoteMember = g_previousRemote;
        g_previousRemote = nullptr;
    }
}
