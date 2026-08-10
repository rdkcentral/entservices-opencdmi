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

#pragma once

#include <atomic>
#include <list>
#include <map>
#include <string>
#include <vector>

#include <interfaces/IContentDecryption.h>

namespace WPEFramework {
namespace Plugin {

class FakeStringIterator : public RPC::IStringIterator {
public:
    explicit FakeStringIterator(const std::vector<std::string>& values)
        : _refCount(1)
        , _values(values)
        , _index(0)
    {
    }

    uint32_t AddRef() const override
    {
        return ++_refCount;
    }

    uint32_t Release() const override
    {
        const uint32_t value = --_refCount;
        if (value == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t interfaceNumber) override
    {
        if ((interfaceNumber == RPC::IStringIterator::ID) ||
            (interfaceNumber == Core::IUnknown::ID)) {
            AddRef();
            return static_cast<RPC::IStringIterator*>(this);
        }

        return nullptr;
    }

    bool Next(std::string& value) override
    {
        if (_index >= _values.size()) {
            return false;
        }
        value = _values[_index++];
        return true;
    }

    bool Previous(std::string& value) override
    {
        if (_index == 0 || _values.empty()) {
            return false;
        }
        --_index;
        value = _values[_index];
        return true;
    }

    void Reset(const uint32_t position) override
    {
        if (position < _values.size()) {
            _index = position;
        } else {
            _index = _values.size();
        }
    }

    bool IsValid() const override
    {
        return (_index < _values.size());
    }

    uint32_t Count() const override
    {
        return static_cast<uint32_t>(_values.size());
    }

    std::string Current() const override
    {
        if (_index < _values.size()) {
            return _values[_index];
        }

        return std::string();
    }

private:
    mutable std::atomic<uint32_t> _refCount;
    std::vector<std::string> _values;
    size_t _index;
};

class FakeContentDecryption : public Exchange::IContentDecryption {
public:
    FakeContentDecryption()
        : _refCount(1)
    {
    }

    void SetSystems(const std::vector<std::string>& systems)
    {
        _systems = systems;
    }

    void SetDesignators(const std::string& system,
                        const std::vector<std::string>& designators)
    {
        _designators[system] = designators;
    }

    uint32_t AddRef() const override
    {
        return ++_refCount;
    }

    uint32_t Release() const override
    {
        const uint32_t value = --_refCount;
        if (value == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t interfaceNumber) override
    {
        if ((interfaceNumber == Exchange::IContentDecryption::ID) ||
            (interfaceNumber == Core::IUnknown::ID)) {
            AddRef();
            return static_cast<Exchange::IContentDecryption*>(this);
        }

        return nullptr;
    }

    uint32_t Initialize(PluginHost::IShell*) override
    {
        return Core::ERROR_NONE;
    }

    void Deinitialize(PluginHost::IShell*) override
    {
    }

    uint32_t Reset() override
    {
        return Core::ERROR_NONE;
    }

    RPC::IStringIterator* Systems() const override
    {
        return new FakeStringIterator(_systems);
    }

    RPC::IStringIterator* Designators(const std::string& keySystem) const override
    {
        auto it = _designators.find(keySystem);
        if (it == _designators.end()) {
            return nullptr;
        }
        return new FakeStringIterator(it->second);
    }

    RPC::IStringIterator* Sessions(const std::string&) const override
    {
        return new FakeStringIterator({});
    }

private:
    mutable std::atomic<uint32_t> _refCount;
    std::vector<std::string> _systems;
    std::map<std::string, std::vector<std::string>> _designators;
};

} // namespace Plugin
} // namespace WPEFramework
