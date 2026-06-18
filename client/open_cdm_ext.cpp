/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#include "Module.h"
#include "open_cdm_ext.h"

#include "open_cdm_impl.h"
#include <map>
#include <string>
#define KEYSYSTEM_DOMAIN_SEPERATOR ";origin="
typedef std::map<const std::string, const std::string> domainKeysystemMap;

static domainKeysystemMap domainMap =  {
#ifdef ENABLE_FANDANGO_WORKAROUND
    {"www-xfinityflex.mgo.com", "com.widevine.fandango"},
#endif
#ifdef ENABLE_AMAZON_WORKAROUND
    {"xfinity.ccast.api.amazonvideo.com", "com.amazon.playready"},
    {"rogers.ccast.api.amazonvideo.com", "com.amazon.playready"},
    {"cox.ccast.api.amazonvideo.com", "com.amazon.playready"},
    {"shaw.ccast.api.amazonvideo.com", "com.amazon.playready"},
    {"videotron.ccast.api.amazonvideo.com", "com.amazon.playready"},
#endif
    };

#define ASSERT_NOT_EXECUTED()                                         \
    {                                                                 \
        fprintf(stderr, "Error: didn't expect to use %s (%s:%d)!!\n", \
            __PRETTY_FUNCTION__, __FILE__, __LINE__);                 \
        abort();                                                      \
    }

OpenCDMError opencdm_parse_keysystem(std::string& keySystemDomain)
{
    OpenCDMError result = ERROR_NONE;

    std::size_t pos = keySystemDomain.find(KEYSYSTEM_DOMAIN_SEPERATOR);
    if (pos!=std::string::npos){
        std::string keySystemStr = keySystemDomain.substr(0, pos);
        std::string domainStr = keySystemDomain.substr(pos + sizeof(KEYSYSTEM_DOMAIN_SEPERATOR)-1);

        domainKeysystemMap::iterator index = domainMap.find(domainStr);

        if(index != domainMap.end()){
            keySystemDomain.assign(index->second);
        }else{
            keySystemDomain.assign(keySystemStr);
        }
    }
    return result;
}

DEPRECATED struct OpenCDMSystem* opencdm_create_system(const char keySystem[])
{
    struct OpenCDMSystem* result = nullptr;
    opencdm_create_system_extended(keySystem, &result);
    return result;
}

OpenCDMError opencdm_create_system_extended(const char keySystem[], struct OpenCDMSystem** system)
{
    ASSERT(system != nullptr);
    *system = nullptr;
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    std::string parsedKeysystem = std::string(keySystem);
    opencdm_parse_keysystem(parsedKeysystem);
    std::string metadata;
    OpenCDMError result = static_cast<OpenCDMError>(accessor->Metadata(parsedKeysystem, metadata));
    if( result == OpenCDMError::ERROR_NONE )
        *system = new OpenCDMSystem(parsedKeysystem.c_str(), metadata);
    return result;
}

OpenCDMError opencdm_system_get_version(struct OpenCDMSystem* system,
    char versionStr[])
{
    ASSERT(system != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    versionStr[0] = '\0';

    std::string versionStdStr = accessor->GetVersionExt(system->keySystem());

    assert(versionStdStr.length() < 64);

    strcpy(versionStr, versionStdStr.c_str());

    return OpenCDMError::ERROR_NONE;
}

OpenCDMError opencdm_system_ext_get_ldl_session_limit(OpenCDMSystem* system,
    uint32_t* ldlLimit)
{
    ASSERT(system != nullptr);
    ASSERT(ldlLimit != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);
    if ((system != nullptr) && (ldlLimit != nullptr)) {
        std::string keySystem = system->keySystem();
        *ldlLimit = accessor->GetLdlSessionLimit(keySystem);
        result = OpenCDMError::ERROR_NONE;
    }

    return (result);
}

uint32_t opencdm_system_ext_is_secure_stop_enabled(
    struct OpenCDMSystem* system)
{
    ASSERT(system != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);
    if (system != nullptr) {
        result = (OpenCDMError)accessor->IsSecureStopEnabled(system->keySystem());
    }

    return (result);
}

OpenCDMError
opencdm_system_ext_enable_secure_stop(struct OpenCDMSystem* system,
    uint32_t use)
{
    ASSERT(system != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);
    if (system != nullptr) {
        result = (OpenCDMError)accessor->EnableSecureStop(system->keySystem(),
        use != 0);
    }

    return (result);
}

uint32_t opencdm_system_ext_reset_secure_stop(struct OpenCDMSystem* system)
{
    ASSERT(system != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);
    if (system != nullptr) {
        result = (OpenCDMError)accessor->ResetSecureStops(system->keySystem());
    }

    return (result);
}

OpenCDMError opencdm_system_ext_get_secure_stop_ids(OpenCDMSystem* system,
    uint8_t ids[],
    uint16_t idsLength,
    uint32_t* count)
{
    ASSERT(system != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);
    if (system != nullptr) {
        result = (OpenCDMError)accessor->GetSecureStopIds(system->keySystem(), ids,
        idsLength, *count);
    }

    return (result);
}

OpenCDMError opencdm_system_ext_get_secure_stop(OpenCDMSystem* system,
    const uint8_t sessionID[],
    uint32_t sessionIDLength,
    uint8_t rawData[],
    uint16_t* rawSize)
{
    ASSERT(system != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);
    if (system != nullptr) {
        result = (OpenCDMError)accessor->GetSecureStop(
        system->keySystem(), sessionID, sessionIDLength, rawData, *rawSize);
    }

    return (result);
}

OpenCDMError opencdm_system_ext_commit_secure_stop(
    OpenCDMSystem* system, const uint8_t sessionID[],
    uint32_t sessionIDLength, const uint8_t serverResponse[],
    uint32_t serverResponseLength)
{
    ASSERT(system != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);
    if (system != nullptr) {
        result = (OpenCDMError)accessor->CommitSecureStop(
        system->keySystem(), sessionID, sessionIDLength, serverResponse,
        serverResponseLength);
    }

    return (result);
}

OpenCDMError opencdm_system_get_drm_time(struct OpenCDMSystem* system,
    uint64_t* time)
{
    ASSERT(system != nullptr);
    ASSERT(time != nullptr);
    OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
    if(!accessor)
        return ERROR_INVALID_ACCESSOR;

    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);

    if ((system != nullptr) && (time != nullptr)) {
        *time = accessor->GetDrmSystemTime(system->keySystem());
        result = OpenCDMError::ERROR_NONE;
    }
    return result;
}

uint32_t
opencdm_session_get_session_id_ext(struct OpenCDMSession* opencdmSession)
{
    uint32_t result(OpenCDMError::ERROR_INVALID_SESSION);
    ASSERT(opencdmSession != nullptr);

    if (opencdmSession != nullptr) {
       result = opencdmSession->SessionIdExt();
    }

    return result;
}

OpenCDMError opencdm_destruct_session_ext(OpenCDMSession* opencdmSession)
{
    OpenCDMError result(OpenCDMError::ERROR_INVALID_SESSION);
    ASSERT(opencdmSession != nullptr);

    if (opencdmSession != nullptr) {
        result = OpenCDMError::ERROR_NONE;
        opencdmSession->Release();
    }

    return result;
}

OpenCDMError
opencdm_session_set_drm_header(struct OpenCDMSession* opencdmSession,
    const uint8_t drmHeader[],
    uint32_t drmHeaderSize)
{
    OpenCDMError result(OpenCDMError::ERROR_INVALID_SESSION);
    ASSERT(opencdmSession != nullptr);

    if (opencdmSession != nullptr) {
        result = (OpenCDMError)opencdmSession->SetDrmHeader(drmHeader, drmHeaderSize);
    }

    return (result);
}

OpenCDMError
opencdm_session_get_challenge_data(struct OpenCDMSession* mOpenCDMSession,
    uint8_t* challenge, uint32_t* challengeSize,
    uint32_t isLDL)
{
    ASSERT(mOpenCDMSession != nullptr);
    ASSERT(challengeSize != nullptr);
    OpenCDMError result(OpenCDMError::ERROR_INVALID_SESSION);

    if ((mOpenCDMSession != nullptr) && (challengeSize != nullptr)) {
        ASSERT((*challengeSize) < 0xFFFF);
        uint16_t realLength = static_cast<uint16_t>(*challengeSize);

        result = static_cast<OpenCDMError>(mOpenCDMSession->GetChallengeDataExt(challenge, realLength, isLDL));
        *challengeSize = realLength;
    }

    return (result);
}

OpenCDMError
opencdm_session_cancel_challenge_data(struct OpenCDMSession* mOpenCDMSession)
{
    ASSERT(mOpenCDMSession != nullptr);
    OpenCDMError result(OpenCDMError::ERROR_INVALID_SESSION);

    if (mOpenCDMSession != nullptr) {
        return (OpenCDMError)mOpenCDMSession->CancelChallengeDataExt();
    }

    return (result);
}

OpenCDMError opencdm_session_store_license_data(
    struct OpenCDMSession* mOpenCDMSession, const uint8_t licenseData[],
    uint32_t licenseDataSize, uint8_t* secureStopId)
{
    ASSERT(mOpenCDMSession != nullptr);
    OpenCDMError result(OpenCDMError::ERROR_INVALID_SESSION);

    if (mOpenCDMSession != nullptr) {
        return (OpenCDMError)mOpenCDMSession->StoreLicenseData(licenseData, licenseDataSize, secureStopId);
    }

    return (result);
}

OpenCDMError opencdm_session_select_key_id(
    struct OpenCDMSession* mOpenCDMSession, uint8_t keyLenght, const uint8_t keyId[])
{
    ASSERT(mOpenCDMSession != nullptr);
    OpenCDMError result(OpenCDMError::ERROR_INVALID_SESSION);

    if (mOpenCDMSession != nullptr) {
        result = (OpenCDMError)mOpenCDMSession->SelectKeyId(keyLenght, keyId);
    }

    return (result);
}

OpenCDMError opencdm_session_clean_decrypt_context(struct OpenCDMSession* mOpenCDMSession)
{
    ASSERT(mOpenCDMSession != nullptr);
    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);

    if (mOpenCDMSession != nullptr) {
        return (OpenCDMError)mOpenCDMSession->CleanDecryptContext();
    }

    return (result);
}



OpenCDMError opencdm_delete_key_store(struct OpenCDMSystem* system)
{
    ASSERT(system != nullptr);
    OpenCDMError result(ERROR_INVALID_ACCESSOR);

    if (system != nullptr) {
        OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
        if(!accessor)
            return ERROR_INVALID_ACCESSOR;

        std::string keySystem = system->keySystem();
        result = (OpenCDMError)accessor->DeleteKeyStore(keySystem);
    }
    return result;
}

OpenCDMError opencdm_delete_secure_store(struct OpenCDMSystem* system)
{
    ASSERT(system != nullptr);
    OpenCDMError result(ERROR_INVALID_ACCESSOR);

    if (system != nullptr) {
        OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
        if(!accessor)
            return ERROR_INVALID_ACCESSOR;

        std::string keySystem = system->keySystem();
        result = (OpenCDMError)accessor->DeleteSecureStore(keySystem);
    }
    return result;
}

OpenCDMError opencdm_get_key_store_hash_ext(struct OpenCDMSystem* system,
    uint8_t keyStoreHash[],
    uint32_t keyStoreHashLength)
{
    ASSERT(system != nullptr);
    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);

    if (system != nullptr) {
        OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
        if(!accessor)
            return ERROR_INVALID_ACCESSOR;

        std::string keySystem = system->keySystem();
        result = (OpenCDMError)accessor->GetKeyStoreHash(keySystem, keyStoreHash,
            keyStoreHashLength);
    }
    return result;
}

OpenCDMError opencdm_get_secure_store_hash_ext(struct OpenCDMSystem* system,
    uint8_t secureStoreHash[],
    uint32_t secureStoreHashLength)
{
    ASSERT(system != nullptr);
    OpenCDMError result(OpenCDMError::ERROR_INVALID_ARG);

    if (system != nullptr) {
        OpenCDMAccessor* accessor = OpenCDMAccessor::Instance();
        if(!accessor)
            return ERROR_INVALID_ACCESSOR;

        std::string keySystem = system->keySystem();
        result = (OpenCDMError)accessor->GetSecureStoreHash(
            keySystem, secureStoreHash, secureStoreHashLength);
    }
    return result;
}

OpenCDMError opencdm_system_ext_get_properties(struct PlayLevels* system, const char* propertiesJSONText) 
{
    using namespace Core ;
    class PlayLevelsJSON : public JSON::Container {
    private:
        PlayLevelsJSON(const PlayLevelsJSON&) = delete;
        PlayLevelsJSON& operator=(const PlayLevelsJSON&) = delete;

    public:
        PlayLevelsJSON()
            : WPEFramework::Core::JSON::Container()
            , _compressedVideo()
            , _uncompressedVideo()
            , _analogVideo()
            , _compressedAudio()
            , _uncompressedAudio()
            , _maxDecodeWidth()
            , _maxDecodeHeight()
        {
            Add(_T("compressed-video"), &_compressedVideo);
            Add(_T("uncompressed-video"), &_uncompressedVideo);
            Add(_T("analog-video"), &_analogVideo);
            Add(_T("compressed-audio"), &_compressedAudio);
            Add(_T("uncompressed-audio"), &_uncompressedAudio);
            Add(_T("max-decode-width"), &_maxDecodeWidth);
            Add(_T("max-decode-height"), &_maxDecodeHeight);
        }

    public:
        JSON::DecUInt16 _compressedVideo;
        JSON::DecUInt16 _uncompressedVideo;
        JSON::DecUInt16 _analogVideo;
        JSON::DecUInt16 _compressedAudio;
        JSON::DecUInt16 _uncompressedAudio;
        JSON::DecUInt32 _maxDecodeWidth;
        JSON::DecUInt32 _maxDecodeHeight;
    };

    ASSERT(system != nullptr);
    ASSERT(propertiesJSONText != nullptr);

    OpenCDMError result(ERROR_INVALID_ACCESSOR);

    if ((system != nullptr) && (propertiesJSONText!= nullptr)) {
        string properties= std::string(propertiesJSONText);
        PlayLevelsJSON playlevelJson;
        playlevelJson.FromString(properties);

        system->_compressedDigitalVideoLevel = playlevelJson._compressedVideo.Value();
        system->_uncompressedDigitalVideoLevel = playlevelJson._uncompressedVideo.Value();
        system->_analogVideoLevel = playlevelJson._analogVideo.Value();
        system->_compressedDigitalAudioLevel = playlevelJson._compressedAudio.Value();
        system->_uncompressedDigitalAudioLevel = playlevelJson._uncompressedAudio.Value();
        system->_maxResDecodeWidth = playlevelJson._maxDecodeWidth.Value();
        system->_maxResDecodeHeight = playlevelJson._maxDecodeHeight.Value();

        result = OpenCDMError::ERROR_NONE;
    }

     return result;
    
}
