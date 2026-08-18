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

#include "FakeServerInterfaces.h"

#include <algorithm>
#include <cstring>

#include "open_cdm_impl.h"

Core::CriticalSection _systemLock;

FakeOpenCDMAccessor::FakeOpenCDMAccessor()
    : OpenCDMAccessor(_T("/tmp/MockOpenCDM"))
    , metadataResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , metadataValue("mock-metadata")
    , versionValue("1.2.3")
    , drmSystemTime(1234)
    , ldlSessionLimit(2)
    , secureStopEnabled(true)
    , isTypeSupportedValue(true)
    , enableSecureStopResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , metricSystemDataResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , serverCertificateResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , supportedRobustnessResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , resetSecureStopsValue(1)
    , secureStopIdsResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , secureStopCount(0)
    , getSecureStopResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , secureStopRawSize(0)
    , commitSecureStopResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , deleteKeyStoreResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , deleteSecureStoreResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , keyStoreHashResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , secureStoreHashResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , keyStoreHashPattern(0xAA)
    , secureStoreHashPattern(0x55)
    , sessionToCreate(nullptr)
    , lastCreateSessionCallback(nullptr)
    , lastCreateSessionLicenseType(0)
{
}

bool FakeOpenCDMAccessor::IsTypeSupported(const std::string&,
                                          const std::string&) const
{
    return isTypeSupportedValue;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::Metricdata(const std::string&,
                                                       uint32_t& length,
                                                       uint8_t buffer[]) const
{
    const uint32_t requiredLength = static_cast<uint32_t>(metricSystemData.size());
    const uint32_t copyLength = std::min(length, requiredLength);

    if ((buffer != nullptr) && (copyLength > 0)) {
        std::memcpy(buffer, metricSystemData.data(), copyLength);
    }
    length = requiredLength;
    return metricSystemDataResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::GetSupportedRobustness(
    const std::string&,
    RPC::IStringIterator*& robustness) const
{
    robustness = nullptr;

    if (supportedRobustnessResult == Exchange::OCDM_RESULT::OCDM_SUCCESS) {
        robustness = new WPEFramework::Plugin::FakeStringIterator(
            supportedRobustnessValues);
    }
    return supportedRobustnessResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::Metadata(const std::string& keySystem,
                                                     std::string& metadata) const
{
    lastMetadataKeySystem = keySystem;
    metadata = metadataValue;
    return metadataResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::SetServerCertificate(
    const std::string&,
    const uint8_t* serverCertificate,
    const uint16_t serverCertificateLength)
{
    lastServerCertificate.clear();
    if ((serverCertificate != nullptr) && (serverCertificateLength > 0)) {
        lastServerCertificate.assign(serverCertificate,
                                     serverCertificate + serverCertificateLength);
    }
    return serverCertificateResult;
}

uint64_t FakeOpenCDMAccessor::GetDrmSystemTime(const std::string&) const
{
    return drmSystemTime;
}

std::string FakeOpenCDMAccessor::GetVersionExt(const std::string&) const
{
    return versionValue;
}

uint32_t FakeOpenCDMAccessor::GetLdlSessionLimit(const std::string&) const
{
    return ldlSessionLimit;
}

bool FakeOpenCDMAccessor::IsSecureStopEnabled(const std::string&)
{
    return secureStopEnabled;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::EnableSecureStop(const std::string&,
                                                             bool)
{
    return enableSecureStopResult;
}

uint32_t FakeOpenCDMAccessor::ResetSecureStops(const std::string&)
{
    return resetSecureStopsValue;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::GetSecureStopIds(const std::string&,
                                                             uint8_t ids[],
                                                             uint16_t idsLength,
                                                             uint32_t& count)
{
    count = secureStopCount;
    const uint16_t copyLength = static_cast<uint16_t>(
        std::min<size_t>(idsLength, secureStopIdsData.size()));
    if ((ids != nullptr) && (copyLength > 0)) {
        std::memcpy(ids, secureStopIdsData.data(), copyLength);
    }
    return secureStopIdsResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::GetSecureStop(const std::string&,
                                                          const uint8_t[],
                                                          uint16_t,
                                                          uint8_t rawData[],
                                                          uint16_t& rawSize)
{
    const uint16_t copyLength = static_cast<uint16_t>(
        std::min<size_t>(rawSize, secureStopRawData.size()));
    if ((rawData != nullptr) && (copyLength > 0)) {
        std::memcpy(rawData, secureStopRawData.data(), copyLength);
    }
    rawSize = secureStopRawSize;
    return getSecureStopResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::CommitSecureStop(const std::string&,
                                                             const uint8_t[],
                                                             uint16_t,
                                                             const uint8_t[],
                                                             uint16_t)
{
    return commitSecureStopResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::DeleteKeyStore(const std::string&)
{
    return deleteKeyStoreResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::DeleteSecureStore(const std::string&)
{
    return deleteSecureStoreResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::GetKeyStoreHash(const std::string&,
                                                            uint8_t keyStoreHash[],
                                                            uint16_t keyStoreHashLength)
{
    if (keyStoreHash != nullptr) {
        std::memset(keyStoreHash, keyStoreHashPattern, keyStoreHashLength);
    }
    return keyStoreHashResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::GetSecureStoreHash(const std::string&,
                                                               uint8_t secureStoreHash[],
                                                               uint16_t secureStoreHashLength)
{
    if (secureStoreHash != nullptr) {
        std::memset(secureStoreHash, secureStoreHashPattern,
                    secureStoreHashLength);
    }
    return secureStoreHashResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::CreateSession(const std::string& keySystem,
                                                         const int32_t licenseType,
                                                         const std::string& initDataType,
                                                         const uint8_t* initData,
                                                         const uint16_t initDataLength,
                                                         const uint8_t* CDMData,
                                                         const uint16_t CDMDataLength,
                                                         Exchange::ISession::ICallback* callback,
                                                         std::string& sessionId,
                                                         Exchange::ISession*& session)
{
    lastCreateSessionKeySystem = keySystem;
    lastCreateSessionLicenseType = licenseType;
    lastCreateSessionInitDataType = initDataType;
    lastCreateSessionInitData.clear();
    if ((initData != nullptr) && (initDataLength > 0)) {
        lastCreateSessionInitData.assign(initData, initData + initDataLength);
    }
    lastCreateSessionCdmData.clear();
    if ((CDMData != nullptr) && (CDMDataLength > 0)) {
        lastCreateSessionCdmData.assign(CDMData, CDMData + CDMDataLength);
    }
    lastCreateSessionCallback = callback;

    if (sessionToCreate == nullptr) {
        sessionToCreate = new FakeSession();
    }

    sessionId = sessionToCreate->SessionId();
    session = sessionToCreate;
    session->AddRef();
    return Exchange::OCDM_RESULT::OCDM_SUCCESS;
}

void FakeOpenCDMAccessor::FireOnKeyMessage(const std::vector<uint8_t>& keyMessage,
                                           const std::string& url) const
{
    if (lastCreateSessionCallback != nullptr) {
        lastCreateSessionCallback->OnKeyMessage(
            keyMessage.empty() ? nullptr : keyMessage.data(),
            static_cast<uint16_t>(keyMessage.size()),
            url);
    }
}

void FakeOpenCDMAccessor::FireOnError(int16_t error,
                                      Exchange::OCDM_RESULT sysError,
                                      const std::string& errorMessage) const
{
    if (lastCreateSessionCallback != nullptr) {
        lastCreateSessionCallback->OnError(error, sysError, errorMessage);
    }
}

void FakeOpenCDMAccessor::FireOnKeyStatusUpdate(
    const std::vector<uint8_t>& keyId,
    Exchange::ISession::KeyStatus status) const
{
    if (lastCreateSessionCallback != nullptr) {
        lastCreateSessionCallback->OnKeyStatusUpdate(
            keyId.empty() ? nullptr : keyId.data(),
            static_cast<uint8_t>(keyId.size()),
            status);
    }
}

void FakeOpenCDMAccessor::FireOnKeyStatusesUpdated() const
{
    if (lastCreateSessionCallback != nullptr) {
        lastCreateSessionCallback->OnKeyStatusesUpdated();
    }
}

FakeOpenCDMAccessor::FakeSession::FakeSession()
    : sessionIdExtValue(77)
    , bufferIdExtValue("buffer-ext")
    , setDrmHeaderResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , getChallengeDataResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , cancelChallengeDataResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , storeLicenseDataResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , selectKeyIdResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , cleanDecryptContextResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , challengeDataLengthOut(0)
    , lastChallengeIsLdl(0)
    , lastSelectedKeyLength(0)
    , revokeCalled(false)
    , closeCalled(false)
    , resetOutputProtectionCalled(false)
    , loadCalled(false)
    , updateCalled(false)
    , loadResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , removeResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , metricResult(Exchange::OCDM_RESULT::OCDM_SUCCESS)
    , statusValue(Exchange::ISession::Usable)
    , statusByKeyValue(Exchange::ISession::Usable)
    , metricSizeOut(0)
    , sessionIdValue("fake-session")
    , metadataValue("fake-session-metadata")
    , _refCount(1)
{
}

uint32_t FakeOpenCDMAccessor::FakeSession::AddRef() const
{
    return ++_refCount;
}

uint32_t FakeOpenCDMAccessor::FakeSession::Release() const
{
    const uint32_t value = --_refCount;
    if (value == 0) {
        delete this;
        return Core::ERROR_DESTRUCTION_SUCCEEDED;
    }
    return Core::ERROR_NONE;
}

void* FakeOpenCDMAccessor::FakeSession::QueryInterface(const uint32_t interfaceNumber)
{
    if ((interfaceNumber == Exchange::ISession::ID) ||
        (interfaceNumber == Core::IUnknown::ID)) {
        AddRef();
        return static_cast<Exchange::ISession*>(this);
    }

    if (interfaceNumber == Exchange::ISessionExt::ID) {
        AddRef();
        return static_cast<Exchange::ISessionExt*>(this);
    }

    return nullptr;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::Load()
{
    loadCalled = true;
    return loadResult;
}

void FakeOpenCDMAccessor::FakeSession::Update(const uint8_t* keyMessage,
                                              const uint16_t keyLength)
{
    updateCalled = true;
    lastUpdatedKeyMessage.clear();
    if ((keyMessage != nullptr) && (keyLength > 0)) {
        lastUpdatedKeyMessage.assign(keyMessage, keyMessage + keyLength);
    }
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::Remove()
{
    return removeResult;
}

std::string FakeOpenCDMAccessor::FakeSession::Metadata() const
{
    return metadataValue;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::Metricdata(uint32_t& bufferSize,
                                                                    uint8_t buffer[]) const
{
    const uint32_t requiredLength = static_cast<uint32_t>(metricData.size());
    const uint32_t copyLength = std::min(bufferSize, requiredLength);
    if ((buffer != nullptr) && (copyLength > 0)) {
        std::memcpy(buffer, metricData.data(), copyLength);
    }
    bufferSize = (metricSizeOut != 0) ? metricSizeOut : requiredLength;
    return metricResult;
}

Exchange::ISession::KeyStatus FakeOpenCDMAccessor::FakeSession::Status() const
{
    return statusValue;
}

Exchange::ISession::KeyStatus FakeOpenCDMAccessor::FakeSession::Status(const uint8_t[], const uint8_t) const
{
    return statusByKeyValue;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::CreateSessionBuffer(std::string& bufferID)
{
    bufferID = bufferIdExtValue;
    return Exchange::OCDM_RESULT::OCDM_SUCCESS;
}

std::string FakeOpenCDMAccessor::FakeSession::BufferId() const
{
    return bufferIdExtValue;
}

std::string FakeOpenCDMAccessor::FakeSession::SessionId() const
{
    return sessionIdValue;
}

void FakeOpenCDMAccessor::FakeSession::Close()
{
    closeCalled = true;
}

void FakeOpenCDMAccessor::FakeSession::ResetOutputProtection()
{
    resetOutputProtectionCalled = true;
}

void FakeOpenCDMAccessor::FakeSession::SetParameter(const std::string& name,
                                                    const std::string& value)
{
    parameters.emplace_back(name, value);
}

void FakeOpenCDMAccessor::FakeSession::Revoke(Exchange::ISession::ICallback*)
{
    revokeCalled = true;
}

uint32_t FakeOpenCDMAccessor::FakeSession::SessionIdExt() const
{
    return sessionIdExtValue;
}

std::string FakeOpenCDMAccessor::FakeSession::BufferIdExt() const
{
    return bufferIdExtValue;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::SetDrmHeader(const uint8_t drmHeader[],
                                                                     uint16_t drmHeaderLength)
{
    lastDrmHeader.assign(drmHeader, drmHeader + drmHeaderLength);
    return setDrmHeaderResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::GetChallengeDataExt(uint8_t* challenge,
                                                                             uint16_t& challengeSize,
                                                                             uint32_t isLDL)
{
    lastChallengeIsLdl = isLDL;
    const uint16_t copyLength = static_cast<uint16_t>(
        std::min<size_t>(challengeSize, challengeData.size()));
    if ((challenge != nullptr) && (copyLength > 0)) {
        std::memcpy(challenge, challengeData.data(), copyLength);
    }
    challengeSize = challengeDataLengthOut;
    return getChallengeDataResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::CancelChallengeDataExt()
{
    return cancelChallengeDataResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::StoreLicenseData(const uint8_t licenseData[],
                                                                         uint16_t licenseDataSize,
                                                                         uint8_t* secureStopId)
{
    lastLicenseData.assign(licenseData, licenseData + licenseDataSize);
    if ((secureStopId != nullptr) && !secureStopIdData.empty()) {
        std::memcpy(secureStopId, secureStopIdData.data(), secureStopIdData.size());
    }
    return storeLicenseDataResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::SelectKeyId(const uint8_t keyLength,
                                                                    const uint8_t keyId[])
{
    lastSelectedKeyLength = keyLength;
    lastSelectedKeyId.assign(keyId, keyId + keyLength);
    return selectKeyIdResult;
}

Exchange::OCDM_RESULT FakeOpenCDMAccessor::FakeSession::CleanDecryptContext()
{
    return cleanDecryptContextResult;
}
