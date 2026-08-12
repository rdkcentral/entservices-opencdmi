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
#include <cstdint>
#include <list>
#include <map>
#include <string>
#include <vector>

#include <interfaces/IContentDecryption.h>

#include "open_cdm_impl.h"

class FakeOpenCDMAccessor : public OpenCDMAccessor {
public:
    class FakeSession;

    FakeOpenCDMAccessor();

    Exchange::OCDM_RESULT metadataResult;
    std::string metadataValue;
    mutable std::string lastMetadataKeySystem;

    std::string versionValue;
    uint64_t drmSystemTime;
    uint32_t ldlSessionLimit;
    bool secureStopEnabled;
    bool isTypeSupportedValue;

    Exchange::OCDM_RESULT enableSecureStopResult;
    Exchange::OCDM_RESULT metricSystemDataResult;
    Exchange::OCDM_RESULT serverCertificateResult;
    Exchange::OCDM_RESULT supportedRobustnessResult;
    uint32_t resetSecureStopsValue;
    Exchange::OCDM_RESULT secureStopIdsResult;
    uint32_t secureStopCount;

    Exchange::OCDM_RESULT getSecureStopResult;
    uint16_t secureStopRawSize;
    Exchange::OCDM_RESULT commitSecureStopResult;
    Exchange::OCDM_RESULT deleteKeyStoreResult;
    Exchange::OCDM_RESULT deleteSecureStoreResult;
    Exchange::OCDM_RESULT keyStoreHashResult;
    Exchange::OCDM_RESULT secureStoreHashResult;

    uint8_t keyStoreHashPattern;
    uint8_t secureStoreHashPattern;
    std::vector<std::string> supportedRobustnessValues;
    std::vector<uint8_t> metricSystemData;
    std::vector<uint8_t> secureStopIdsData;
    std::vector<uint8_t> secureStopRawData;
    FakeSession* sessionToCreate;
    Exchange::ISession::ICallback* lastCreateSessionCallback;

    std::string lastCreateSessionKeySystem;
    int32_t lastCreateSessionLicenseType;
    std::string lastCreateSessionInitDataType;
    std::vector<uint8_t> lastCreateSessionInitData;
    std::vector<uint8_t> lastCreateSessionCdmData;
    std::vector<uint8_t> lastServerCertificate;

    bool IsTypeSupported(const std::string& keySystem,
                         const std::string& mimeType) const override;
    Exchange::OCDM_RESULT Metricdata(const std::string& keySystem,
                                     uint32_t& length,
                                     uint8_t buffer[]) const override;
    Exchange::OCDM_RESULT GetSupportedRobustness(
        const std::string& keySystem,
        RPC::IStringIterator*& robustness) const override;
    Exchange::OCDM_RESULT Metadata(const std::string& keySystem,
                                   std::string& metadata) const override;
    Exchange::OCDM_RESULT SetServerCertificate(
        const std::string& keySystem,
        const uint8_t* serverCertificate,
        const uint16_t serverCertificateLength) override;
    uint64_t GetDrmSystemTime(const std::string& keySystem) const override;
    std::string GetVersionExt(const std::string& keySystem) const override;
    uint32_t GetLdlSessionLimit(const std::string& keySystem) const override;
    bool IsSecureStopEnabled(const std::string& keySystem) override;
    Exchange::OCDM_RESULT EnableSecureStop(const std::string& keySystem,
                                           bool enable) override;
    uint32_t ResetSecureStops(const std::string& keySystem) override;
    Exchange::OCDM_RESULT GetSecureStopIds(const std::string& keySystem,
                                           uint8_t ids[],
                                           uint16_t idsLength,
                                           uint32_t& count) override;
    Exchange::OCDM_RESULT GetSecureStop(const std::string& keySystem,
                                        const uint8_t sessionID[],
                                        uint16_t sessionIDLength,
                                        uint8_t rawData[],
                                        uint16_t& rawSize) override;
    Exchange::OCDM_RESULT CommitSecureStop(const std::string& keySystem,
                                           const uint8_t sessionID[],
                                           uint16_t sessionIDLength,
                                           const uint8_t serverResponse[],
                                           uint16_t serverResponseLength) override;
    Exchange::OCDM_RESULT DeleteKeyStore(const std::string& keySystem) override;
    Exchange::OCDM_RESULT DeleteSecureStore(const std::string& keySystem) override;
    Exchange::OCDM_RESULT GetKeyStoreHash(const std::string& keySystem,
                                          uint8_t keyStoreHash[],
                                          uint16_t keyStoreHashLength) override;
    Exchange::OCDM_RESULT GetSecureStoreHash(const std::string& keySystem,
                                             uint8_t secureStoreHash[],
                                             uint16_t secureStoreHashLength) override;
    Exchange::OCDM_RESULT CreateSession(const std::string& keySystem,
                                        const int32_t licenseType,
                                        const std::string& initDataType,
                                        const uint8_t* initData,
                                        const uint16_t initDataLength,
                                        const uint8_t* CDMData,
                                        const uint16_t CDMDataLength,
                                        Exchange::ISession::ICallback* callback,
                                        std::string& sessionId,
                                        Exchange::ISession*& session) override;

    void FireOnKeyMessage(const std::vector<uint8_t>& keyMessage,
                          const std::string& url) const;
    void FireOnError(int16_t error,
                     Exchange::OCDM_RESULT sysError,
                     const std::string& errorMessage) const;
    void FireOnKeyStatusUpdate(const std::vector<uint8_t>& keyId,
                               Exchange::ISession::KeyStatus status) const;
    void FireOnKeyStatusesUpdated() const;
};

class FakeOpenCDMAccessor::FakeSession
    : public Exchange::ISession
    , public Exchange::ISessionExt {
public:
    FakeSession();

    uint32_t sessionIdExtValue;
    std::string bufferIdExtValue;
    Exchange::OCDM_RESULT setDrmHeaderResult;
    Exchange::OCDM_RESULT getChallengeDataResult;
    Exchange::OCDM_RESULT cancelChallengeDataResult;
    Exchange::OCDM_RESULT storeLicenseDataResult;
    Exchange::OCDM_RESULT selectKeyIdResult;
    Exchange::OCDM_RESULT cleanDecryptContextResult;

    std::vector<uint8_t> challengeData;
    uint16_t challengeDataLengthOut;
    uint32_t lastChallengeIsLdl;
    std::vector<uint8_t> lastDrmHeader;
    std::vector<uint8_t> lastLicenseData;
    std::vector<uint8_t> lastSelectedKeyId;
    uint8_t lastSelectedKeyLength;
    std::vector<uint8_t> secureStopIdData;
    bool revokeCalled;
    bool closeCalled;
    bool resetOutputProtectionCalled;
    bool loadCalled;
    bool updateCalled;
    Exchange::OCDM_RESULT loadResult;
    Exchange::OCDM_RESULT removeResult;
    Exchange::OCDM_RESULT metricResult;
    Exchange::ISession::KeyStatus statusValue;
    Exchange::ISession::KeyStatus statusByKeyValue;
    std::vector<uint8_t> metricData;
    uint32_t metricSizeOut;
    std::vector<uint8_t> lastUpdatedKeyMessage;
    std::vector<std::pair<std::string, std::string>> parameters;
    std::string sessionIdValue;
    std::string metadataValue;

    uint32_t AddRef() const override;
    uint32_t Release() const override;
    void* QueryInterface(const uint32_t interfaceNumber) override;

    Exchange::OCDM_RESULT Load() override;
    void Update(const uint8_t* keyMessage, const uint16_t keyLength) override;
    Exchange::OCDM_RESULT Remove() override;
    std::string Metadata() const override;
    Exchange::OCDM_RESULT Metricdata(uint32_t& bufferSize, uint8_t buffer[]) const override;
    Exchange::ISession::KeyStatus Status() const override;
    Exchange::ISession::KeyStatus Status(const uint8_t keyID[], const uint8_t keyIDLength) const override;
    Exchange::OCDM_RESULT CreateSessionBuffer(std::string& bufferID) override;
    std::string BufferId() const override;
    std::string SessionId() const override;
    void Close() override;
    void ResetOutputProtection() override;
    void SetParameter(const std::string& name, const std::string& value) override;
    void Revoke(Exchange::ISession::ICallback* callback) override;

    uint32_t SessionIdExt() const override;
    std::string BufferIdExt() const override;
    Exchange::OCDM_RESULT SetDrmHeader(const uint8_t drmHeader[], uint16_t drmHeaderLength) override;
    Exchange::OCDM_RESULT GetChallengeDataExt(uint8_t* challenge, uint16_t& challengeSize, uint32_t isLDL) override;
    Exchange::OCDM_RESULT CancelChallengeDataExt() override;
    Exchange::OCDM_RESULT StoreLicenseData(const uint8_t licenseData[], uint16_t licenseDataSize, uint8_t* secureStopId) override;
    Exchange::OCDM_RESULT SelectKeyId(const uint8_t keyLength, const uint8_t keyId[]) override;
    Exchange::OCDM_RESULT CleanDecryptContext() override;

private:
    mutable std::atomic<uint32_t> _refCount;
};

void InstallFakeAccessor(OpenCDMAccessor* accessor);
void UninstallFakeAccessor();

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
