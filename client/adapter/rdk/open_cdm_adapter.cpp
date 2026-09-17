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

#include "open_cdm_adapter.h"
#undef EXTERNAL  // open_cdm.h defines EXTERNAL; WPEFramework Portability.h redefines it
#include "open_cdm_impl.h"

#include "Module.h"
#include <gst/gst.h>
#include <gst/base/gstbytereader.h>
#include <dlfcn.h>

#include <cstdint>
#include <cstring>

#include <gst_svp_meta.h>
#include "../CapsParser.h"

typedef gboolean (*svp_set_value_fn_t)(void *, const char *, void *, const size_t);
static svp_set_value_fn_t s_svpSetValueFn = nullptr;

namespace
{
constexpr uint32_t kMaxIvSize = 16;

} // namespace


EXTERNAL OpenCDMError opencdm_gstreamer_transform_caps(GstCaps** caps)
{
    OpenCDMError result (ERROR_NONE);

    if(!gst_svp_ext_transform_caps(caps, TRUE))
        result = ERROR_UNKNOWN;

    return (result);
}

bool swapIVBytes(uint8_t *mappedIV,uint32_t mappedIVSize)
{
    for (uint32_t i = 0; i < mappedIVSize / 2; i++) {
        uint8_t buf = mappedIV[i];
        mappedIV[i] = mappedIV[mappedIVSize - i - 1];
        mappedIV[mappedIVSize - i - 1] = buf;
    }

    return true;
}

uint32_t opencdm_construct_session_private(struct OpenCDMSession* session, void* &pvtData)
{
    bool success = gst_svp_ext_get_context(&pvtData, Server, (unsigned int)session);
    if (success) {
        TRACE_L1("Initialized SVP context for server side ID = %X\n",(unsigned int)session);
        char buf[25] = { 0 };
        snprintf(buf, 25, "%X", (unsigned int)session);
        session->SetParameter("rpcId", buf);

        if (!s_svpSetValueFn) {
            s_svpSetValueFn = (svp_set_value_fn_t)dlsym(RTLD_DEFAULT, "gst_svp_ext_context_process_store_set_value");
        }

        return 0;
    }
    return 1;
}

uint32_t opencdm_destruct_session_private(struct OpenCDMSession* session, void* &pvtData)
{
    bool success = gst_svp_ext_free_context(pvtData);
    return (success ? 0 : 1);
}

OpenCDMError opencdm_gstreamer_session_decrypt_once(struct OpenCDMSession* session, GstBuffer* buffer, GstBuffer* subSample, const uint32_t subSampleCount,
                                                     GstBuffer* IV, GstBuffer* keyID, uint32_t initWithLast15)
{
    OpenCDMError result (ERROR_INVALID_SESSION);

    if (session != nullptr) {
        GstMapInfo dataMap = { 0 };
        if (gst_buffer_map(buffer, &dataMap, (GstMapFlags) GST_MAP_READWRITE) == false) {
            fprintf(stderr, "Invalid buffer.\n");
            return (ERROR_INVALID_DECRYPT_BUFFER);
        }

        media_type mediaType = Data;

        if(subSample == NULL && IV == NULL && keyID == NULL) {
            // no encrypted data, skip decryption...
            // But still need to transform buffer for SVP support
            gst_buffer_svp_transform_from_cleardata(session->SessionPrivateData(), buffer, mediaType);
            gst_buffer_unmap(buffer, &dataMap);
            return(ERROR_NONE);
        }

        GstMapInfo ivMap = { 0 };

        if (gst_buffer_map(IV, &ivMap, (GstMapFlags) GST_MAP_READ) == false) {
            gst_buffer_unmap(buffer, &dataMap);
            fprintf(stderr, "Invalid IV buffer.\n");
            return (ERROR_INVALID_DECRYPT_BUFFER);
        }

        GstMapInfo keyIDMap = { 0 };

        if (gst_buffer_map(keyID, &keyIDMap, (GstMapFlags) GST_MAP_READ) == false) {
            gst_buffer_unmap(buffer, &dataMap);
            gst_buffer_unmap(IV, &ivMap);
            fprintf(stderr, "Invalid keyID buffer.\n");
            return (ERROR_INVALID_DECRYPT_BUFFER);
        }

        //Set the Encryption Scheme and Pattern to defaults.
        EncryptionScheme encScheme = AesCtr_Cenc;
        EncryptionPattern pattern = {0, 0};

        //Lets try to get Enc Scheme and Pattern from the Protection Metadata.
        GstProtectionMeta* protectionMeta = reinterpret_cast<GstProtectionMeta*>(gst_buffer_get_protection_meta(buffer));
        if (protectionMeta != NULL) {
            const char* cipherModeBuf = gst_structure_get_string(protectionMeta->info, "cipher-mode");
            if(g_strcmp0(cipherModeBuf,"cbcs") == 0) {
                encScheme = AesCbc_Cbcs;
            }

            gst_structure_get_uint(protectionMeta->info, "crypt_byte_block", &pattern.encrypted_blocks);
            gst_structure_get_uint(protectionMeta->info, "skip_byte_block", &pattern.clear_blocks);
        }

        uint8_t *mappedData = reinterpret_cast<uint8_t* >(dataMap.data);
        uint32_t mappedDataSize = static_cast<uint32_t >(dataMap.size);
        uint8_t *mappedIV = reinterpret_cast<uint8_t* >(ivMap.data);
        uint32_t mappedIVSize = static_cast<uint32_t >(ivMap.size);

        uint8_t *mappedKeyID = reinterpret_cast<uint8_t* >(keyIDMap.data);
        uint32_t mappedKeyIDSize = static_cast<uint32_t >(keyIDMap.size);

        if (subSample != nullptr) {
            GstMapInfo sampleMap = { 0 };

            if (gst_buffer_map(subSample, &sampleMap, GST_MAP_READ) == false) {
                fprintf(stderr, "Invalid subsample buffer.\n");
                gst_buffer_unmap(keyID, &keyIDMap);
                gst_buffer_unmap(IV, &ivMap);
                gst_buffer_unmap(buffer, &dataMap);
                return (ERROR_INVALID_DECRYPT_BUFFER);
            }
            uint8_t *mappedSubSample = reinterpret_cast<uint8_t* >(sampleMap.data);
            uint32_t mappedSubSampleSize = static_cast<uint32_t >(sampleMap.size);

            GstByteReader* reader = gst_byte_reader_new(mappedSubSample, mappedSubSampleSize);
            uint16_t inClear = 0;
            uint32_t inEncrypted = 0;
            uint32_t totalEncrypted = 0;
            for (unsigned int position = 0; position < subSampleCount; position++) {

                gst_byte_reader_get_uint16_be(reader, &inClear);
                gst_byte_reader_get_uint32_be(reader, &inEncrypted);
                totalEncrypted += inEncrypted;
            }
            gst_byte_reader_set_pos(reader, 0);

            if(totalEncrypted > 0)
            {

                uint8_t* svpData;
                gsize dataBlockSize = gst_svp_allocate_data_block(session->SessionPrivateData(), (void**) &svpData, totalEncrypted, totalEncrypted);

                uint8_t* encryptedDataIter = reinterpret_cast<uint8_t *>(gst_svp_header_get_start_of_data(session->SessionPrivateData(), svpData));

                uint32_t index = 0;
                for (unsigned int position = 0; position < subSampleCount; position++) {

                    gst_byte_reader_get_uint16_be(reader, &inClear);
                    gst_byte_reader_get_uint32_be(reader, &inEncrypted);

                    memcpy(encryptedDataIter, mappedData + index + inClear, inEncrypted);
                    index += inClear + inEncrypted;
                    encryptedDataIter += inEncrypted;
                }
                gst_byte_reader_set_pos(reader, 0);

                GstPerf* ocdm_perf = new GstPerf("opencdm_session_decrypt_subsample");
                result = opencdm_session_decrypt(session, svpData, dataBlockSize, encScheme, pattern, mappedIV, mappedIVSize,
                                                 mappedKeyID, mappedKeyIDSize, initWithLast15);
                delete ocdm_perf;

                if(result == ERROR_NONE) {
                    GstPerf* svpTransform_perf1 = new GstPerf("opencdm_svp_transform_subsample");
                    gst_buffer_append_svp_transform(session->SessionPrivateData(), buffer, subSample, subSampleCount, svpData);
                    delete svpTransform_perf1;
                }
                gst_svp_free_data_block(session->SessionPrivateData(), svpData);
            } else {
                // no encrypted data, skip decryption...
                // But still need to transform buffer for SVP support
                gst_buffer_svp_transform_from_cleardata(session->SessionPrivateData(), buffer, mediaType);
                result = ERROR_NONE;
            }
            gst_byte_reader_free(reader);
            gst_buffer_unmap(subSample, &sampleMap);
        } else {
            uint8_t* encryptedData = NULL;
            uint8_t* svpData = NULL;

            uint32_t dataBlockSize = gst_svp_allocate_data_block(session->SessionPrivateData(), (void**) &svpData, mappedDataSize, mappedDataSize);

            // Adjust data start after header
            encryptedData = reinterpret_cast<uint8_t *>(gst_svp_header_get_start_of_data(session->SessionPrivateData(), svpData));

            memcpy(encryptedData, mappedData, mappedDataSize);

            GstPerf* ocdm_perf = new GstPerf("opencdm_session_decrypt_no_subsample");
            result = opencdm_session_decrypt(session, svpData, dataBlockSize, encScheme, pattern, mappedIV, mappedIVSize,
                                             mappedKeyID, mappedKeyIDSize, initWithLast15);
            delete ocdm_perf;

            if(result == ERROR_NONE){
                GstPerf* svpTransform_perf2 = new GstPerf("opencdm_svp_transform_no_subsample");
                gst_buffer_append_svp_transform(session->SessionPrivateData(), buffer, NULL, mappedDataSize, svpData);
                delete svpTransform_perf2;
            }
            gst_svp_free_data_block(session->SessionPrivateData(), svpData);
        }

        if (keyID != nullptr) {
           gst_buffer_unmap(keyID, &keyIDMap);
        }

        gst_buffer_unmap(IV, &ivMap);
        gst_buffer_unmap(buffer, &dataMap);
    }

    return (result);
}

OpenCDMError extend_subsample_map(SubSampleInfo** subSampleInfoPtr, unsigned int* subSampleCount, uint32_t bufferSize, uint32_t totalBytes)
{
    SubSampleInfo* subSampleInfoPtrLocal = *subSampleInfoPtr;

    RDKPerf perf_subsample(__FUNCTION__);

    // Add an extra subsample(s) entry to account for the size mismatch
    uint32_t additional_bytes = bufferSize - totalBytes;
    uint32_t extra_subsamples = 0;
    // Calculate how many extra subsamples are needed to fit 16bit clear data size
    while (additional_bytes > 0) {
        uint16_t clear_bytes = 0;
        if (additional_bytes > 0xFFFF) {
            clear_bytes = 0xFFFF;
        } else {
            clear_bytes = static_cast<uint16_t>(additional_bytes);
        }
        additional_bytes -= clear_bytes;
        extra_subsamples += 1;
    }
    uint32_t newSubSampleCount = *subSampleCount + extra_subsamples;
    SubSampleInfo* tmp = reinterpret_cast<SubSampleInfo*>(realloc(subSampleInfoPtrLocal, newSubSampleCount * sizeof(SubSampleInfo)));
    if(tmp != nullptr) {
        subSampleInfoPtrLocal = tmp;
        *subSampleCount = newSubSampleCount;
        while(extra_subsamples > 1) {
            // Fill in the extra subsample entries with max clear size
            subSampleInfoPtrLocal[*subSampleCount - extra_subsamples].clear_bytes = 0xFFFF;
            subSampleInfoPtrLocal[*subSampleCount - extra_subsamples].encrypted_bytes = 0;
            extra_subsamples--;
            totalBytes += 0xFFFF;
        }
        subSampleInfoPtrLocal[*subSampleCount - 1].clear_bytes = bufferSize - totalBytes;
        subSampleInfoPtrLocal[*subSampleCount - 1].encrypted_bytes = 0;
    } else {
        RDKPerf perf_alloc_fail("SubsampleSizeFixAllocFail");
        TRACE_L1("extend_subsample_map: Memory allocation failed when adjusting subsample mapping.");

        return ERROR_OUT_OF_MEMORY;
    }
    // Copy the newly allocated subsample info back to the caller
    *subSampleInfoPtr = subSampleInfoPtrLocal;
    return ERROR_NONE;
}

OpenCDMError validate_subsample_map(SubSampleInfo** subSampleInfoPtr, unsigned int* subSampleCount, uint32_t frameSize, uint32_t totalMappedBytes)
{
    OpenCDMError retVal = ERROR_NONE;

    if(frameSize == 0 || subSampleInfoPtr == nullptr || *subSampleInfoPtr == nullptr || subSampleCount == nullptr || *subSampleCount == 0) {
        // Unable to fully validate the subsample map, but nothing to validate against
       // return ERROR_NONE and use the existing code path
        return retVal;
    }
    if(frameSize == totalMappedBytes) {
        // Perfect match, no need to adjust anything
        retVal = ERROR_NONE;
    }
    else if(frameSize > totalMappedBytes) {
        TRACE_L3("opencdm_gstreamer_session_decrypt_buffer: Subsample mapping size mismatch. FrameSize: %u, TotalBytes from SubsampleInfo: %u",
                 frameSize, totalMappedBytes);
        retVal = extend_subsample_map(subSampleInfoPtr, subSampleCount, frameSize, totalMappedBytes);
    }
    else if(frameSize < totalMappedBytes) {
        TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: Subsample mapping size exceeds data size. FrameSize: %u, TotalBytes from SubsampleInfo: %u",
                 frameSize, totalMappedBytes);
        retVal = ERROR_INVALID_DECRYPT_BUFFER;
    }

    return retVal;
}

OpenCDMError opencdm_gstreamer_session_decrypt_buffer_once(struct OpenCDMSession* session, GstBuffer* buffer, GstCaps* caps) {

    OpenCDMError result (ERROR_INVALID_SESSION);

    if (session != nullptr) {

        GstMapInfo dataMap = { 0 };
        if (gst_buffer_map(buffer, &dataMap, (GstMapFlags) GST_MAP_READWRITE) == false) {

            TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: Invalid buffer.");
            result = ERROR_INVALID_DECRYPT_BUFFER;
            goto exit;
        }

        media_type mediaType = Data;
        uint8_t *mappedData = reinterpret_cast<uint8_t* >(dataMap.data);
        uint32_t mappedDataSize = static_cast<uint32_t >(dataMap.size);

        //Check if Protection Metadata is available in Buffer
        GstProtectionMeta* protectionMeta = reinterpret_cast<GstProtectionMeta*>(gst_buffer_get_protection_meta(buffer));
        if (protectionMeta != nullptr) {
            const GValue* value;

            //Get Subsample mapping
            unsigned subSampleCount = 0;
            if (!gst_structure_get_uint(protectionMeta->info, "subsample_count", &subSampleCount)) {
                TRACE_L1("No Subsample Count.\n");
            }

            GstBuffer* subSample = nullptr;
            GstMapInfo sampleMap = { 0 };
            uint8_t *mappedSubSample = nullptr;
            uint32_t mappedSubSampleSize = 0;
            if (subSampleCount > 0) {
                value = gst_structure_get_value(protectionMeta->info, "subsamples");
                if (!value) {
                    TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: No subsample buffer.");
                    gst_buffer_unmap(buffer, &dataMap);
                    result = ERROR_INVALID_DECRYPT_BUFFER;
                    goto exit;
                }

                subSample = gst_value_get_buffer(value);
                if (subSample != nullptr && gst_buffer_map(subSample, &sampleMap, GST_MAP_READ) == false) {
                    TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: Invalid subsample buffer.");
                    gst_buffer_unmap(buffer, &dataMap);
                    result = ERROR_INVALID_DECRYPT_BUFFER;
                    goto exit;
                }
                mappedSubSample = reinterpret_cast<uint8_t* >(sampleMap.data);
                mappedSubSampleSize = static_cast<uint32_t >(sampleMap.size);
            }

            //Get IV
            value = gst_structure_get_value(protectionMeta->info, "iv");
            if (!value) {
                TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: Missing IV buffer.");
                gst_buffer_unmap(buffer, &dataMap);
                gst_buffer_unmap(subSample, &sampleMap);
                result = ERROR_INVALID_DECRYPT_BUFFER;
                goto exit;
            }
            GstBuffer* IV = gst_value_get_buffer(value);

            GstMapInfo ivMap = { 0 };
            if (IV != nullptr && gst_buffer_map(IV, &ivMap, (GstMapFlags) GST_MAP_READ) == false) {
                TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: Invalid IV buffer.");
                gst_buffer_unmap(buffer, &dataMap);
                gst_buffer_unmap(subSample, &sampleMap);
                result = ERROR_INVALID_DECRYPT_BUFFER;
                goto exit;
            }
            uint8_t *mappedIV = reinterpret_cast<uint8_t* >(ivMap.data);
            uint32_t mappedIVSize = static_cast<uint32_t >(ivMap.size);

            unsigned InitWithLast15 = 0;
            if (!gst_structure_get_uint(protectionMeta->info, "initWithLast15", &InitWithLast15)) {
                TRACE_L3("opencdm_gstreamer_session_decrypt_buffer: Missing initWithLast15 value.");
            }
            if (InitWithLast15 == 1) {
                swapIVBytes(mappedIV,mappedIVSize);
            }

            //Get Key ID
            value = gst_structure_get_value(protectionMeta->info, "kid");
            if (!value) {
                TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: Missing KeyId buffer.");
                gst_buffer_unmap(buffer, &dataMap);
                gst_buffer_unmap(subSample, &sampleMap);
                gst_buffer_unmap(IV, &ivMap);
                result = ERROR_INVALID_DECRYPT_BUFFER;
                goto exit;
            }

            GstBuffer* keyID = gst_value_get_buffer(value);

            uint8_t *mappedKeyID = nullptr;
            uint32_t mappedKeyIDSize = 0;
            GstMapInfo keyIDMap = { 0 };
            if (keyID != nullptr && gst_buffer_map(keyID, &keyIDMap, (GstMapFlags) GST_MAP_READ) == false) {
                TRACE_L1("Invalid keyID buffer.");
                gst_buffer_unmap(buffer, &dataMap);
                gst_buffer_unmap(subSample, &sampleMap);
                gst_buffer_unmap(IV, &ivMap);
                result = ERROR_INVALID_DECRYPT_BUFFER;
                goto exit;
            }
            mappedKeyID = reinterpret_cast<uint8_t* >(keyIDMap.data);
            mappedKeyIDSize = static_cast<uint32_t >(keyIDMap.size);

            std::string perfString(__FUNCTION__);
            //Get Stream Properties from GstCaps
            MediaProperties streamProperties = {0};
            WPEFramework::Plugin::CapsParser capsParser;

            bool isSecureMemoryDisabled = false;
            if(caps != nullptr){
                gchar *capsStr = gst_caps_to_string (caps);
                if (capsStr != nullptr) {
                    capsParser.Parse(reinterpret_cast<const uint8_t*>(capsStr), strlen(capsStr));
                    streamProperties.height = capsParser.GetHeight();
                    streamProperties.width = capsParser.GetWidth();
                    switch (capsParser.GetMediaType()) {
                        case CDMi::MediaType::Video:
                            streamProperties.media_type = MediaType_Video;
                            mediaType = Video;
                            if (capsParser.IsSecureMemoryDisabled()) {
                                isSecureMemoryDisabled = true;
                                TRACE_L1("Secure Memory Preallocation disabled as decrypt-to-host is set\n");
                            }
                            perfString += "_Video";
                        break;

                        case CDMi::MediaType::Audio:
                            streamProperties.media_type = MediaType_Audio;
                            mediaType = Audio;
                            perfString += "_Audio";
                        break;

                        case CDMi::MediaType::Data:
                            streamProperties.media_type = MediaType_Data;
                            mediaType = Data;
                            perfString += "_Data";
                        break;

                        default:
                            streamProperties.media_type = MediaType_Unknown;
                            perfString += "_Unknown";
                        break;
                    }

                    if (subSample == nullptr && IV == nullptr && keyID == nullptr) {
                       perfString += "_clearData";
                    }

                    g_free(capsStr);
                } else {
                    perfString += "_NoGstCaps";
                    TRACE_L1("Could not convert caps to string\n");
                }
            }
            RDKPerf perf(perfString.c_str());

            if (s_svpSetValueFn && session->NeedsDecryptToHostUpdate(capsParser.IsSecureMemoryDisabled())) {
                const gboolean decryptToHost = capsParser.IsSecureMemoryDisabled() ? TRUE : FALSE;
                if (!s_svpSetValueFn(session->SessionPrivateData(),
                                "decryptToHost",
                                (void*)&decryptToHost,
                                sizeof(decryptToHost))) {
                    TRACE_L1("Failed to set decryptToHost=%s in SVP context\n", decryptToHost ? "true" : "false");
                }
            }

            if (subSample == nullptr && IV == nullptr && keyID == nullptr) {
            // no encrypted data, skip decryption...
            // But still need to transform buffer for SVP support
                gst_buffer_svp_transform_from_cleardata(session->SessionPrivateData(), buffer, mediaType);
                gst_buffer_unmap(buffer, &dataMap);
                return(ERROR_NONE);
            }

            //Get Encryption Scheme and Pattern
            EncryptionScheme encScheme = AesCtr_Cenc;
            EncryptionPattern pattern = {0, 0};
            const char* cipherModeBuf = gst_structure_get_string(protectionMeta->info, "cipher-mode");
            if(g_strcmp0(cipherModeBuf,"cbcs") == 0) {
                encScheme = AesCbc_Cbcs;
            }
            gst_structure_get_uint(protectionMeta->info, "crypt_byte_block", &pattern.encrypted_blocks);
            gst_structure_get_uint(protectionMeta->info, "skip_byte_block", &pattern.clear_blocks);

            //Create a SubSampleInfo Array with mapping
            SubSampleInfo * subSampleInfoPtr = nullptr;
            uint32_t total_encrypted_bytes = 0;
            uint32_t total_mapped_bytes = 0;
            if (subSample != nullptr) {
                GstByteReader* reader = gst_byte_reader_new(mappedSubSample, mappedSubSampleSize);
                subSampleInfoPtr = reinterpret_cast<SubSampleInfo*>(malloc(subSampleCount * sizeof(SubSampleInfo)));
                for (unsigned int position = 0; position < subSampleCount; position++) {

                    gst_byte_reader_get_uint16_be(reader, &subSampleInfoPtr[position].clear_bytes);
                    gst_byte_reader_get_uint32_be(reader, &subSampleInfoPtr[position].encrypted_bytes);
                    total_encrypted_bytes += subSampleInfoPtr[position].encrypted_bytes;
                    total_mapped_bytes += subSampleInfoPtr[position].clear_bytes + subSampleInfoPtr[position].encrypted_bytes;
                }
                // Validate that the subsample mapping matches the data size
                result = validate_subsample_map(&subSampleInfoPtr, &subSampleCount, mappedDataSize, total_mapped_bytes);
                if(result != ERROR_NONE) {
                    TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: Failed to correct subsample mapping.");
                    gst_buffer_unmap(buffer, &dataMap);
                    gst_buffer_unmap(subSample, &sampleMap);
                    gst_buffer_unmap(IV, &ivMap);
                    gst_buffer_unmap(keyID, &keyIDMap);
                    free(subSampleInfoPtr);
                    subSampleInfoPtr = nullptr;
                    gst_byte_reader_free(reader);
                    goto exit;
                 }
                gst_byte_reader_set_pos(reader, 0);
                gst_byte_reader_free(reader);
            } else {
                 total_encrypted_bytes = mappedDataSize;
            }

            SampleInfo sampleInfo;
            sampleInfo.subSample = subSampleInfoPtr;
            sampleInfo.subSampleCount = subSampleCount;
            sampleInfo.scheme = encScheme;
            sampleInfo.pattern.clear_blocks = pattern.clear_blocks;
            sampleInfo.pattern.encrypted_blocks = pattern.encrypted_blocks;
            sampleInfo.iv = mappedIV;
            sampleInfo.ivLength = mappedIVSize;
            sampleInfo.keyId = mappedKeyID;
            sampleInfo.keyIdLength = mappedKeyIDSize;

            if(total_encrypted_bytes > 0) {
               uint8_t* svpData;

              const gboolean needSecureMemoryPrealloc = (streamProperties.media_type == MediaType_Video)
                                                      && gst_svp_context_supports_memory_prealloc(session->SessionPrivateData())
                                                      && (!isSecureMemoryDisabled);

              uint32_t dataBlockSize = gst_svp_allocate_data_block(session->SessionPrivateData(),
                                                                   (void**) &svpData,
                                                                   mappedDataSize,
                                                                   mappedDataSize,
                                                                   needSecureMemoryPrealloc);

               void * encryptedData = reinterpret_cast<uint8_t *>(gst_svp_header_get_start_of_data(session->SessionPrivateData(), svpData));

               memcpy(encryptedData, mappedData, mappedDataSize);

               TokenType tokenType = TokenType::InPlace;
               if (!gst_svp_header_get_field(session->SessionPrivateData(), svpData, SvpHeaderFieldName::Type, (uint32_t*) &tokenType))
               {
                  TRACE_L1("Failed to get type from SVP header");
               }

               const bool isRevokedAllocation = needSecureMemoryPrealloc 
                                                && tokenType != TokenType::InPlace
                                                && tokenType != TokenType::Handle
                                                && tokenType != TokenType::PreAllocatedHandle;

               if (!isRevokedAllocation)
               {
                if(isSecureMemoryDisabled)
                {
                   TRACE_L1("Secure Memory Preallocation disabled, Setting TokenType to InPlace");
                   gst_svp_header_set_field(session->SessionPrivateData(), svpData, SvpHeaderFieldName::Type, (uint32_t)TokenType::InPlace);
                }

                GstPerf* ocdm_perf = new GstPerf("opencdm_session_decrypt_v2");
                result = opencdm_session_decrypt_v2(session,
                                                    svpData,
                                                    dataBlockSize,
                                                    &sampleInfo,
                                                    &streamProperties);
                delete ocdm_perf;
               }
               else
               {
                    TRACE_L1("Skipping decrypt as resources have been revoked");
                    result = ERROR_NONE;
               }

               if(result == ERROR_NONE) {
                  GstPerf* svpTransform_perf3 = new GstPerf("opencdm_svp_transform_subsample");
                  gst_buffer_append_svp_transform(session->SessionPrivateData(), buffer, subSample, subSampleCount, svpData, mappedDataSize);
                  delete svpTransform_perf3;
               }
               gst_svp_free_data_block(session->SessionPrivateData(), svpData);
           } else {
               // no encrypted data, skip decryption...
               // But still need to transform buffer for SVP support
               gst_buffer_svp_transform_from_cleardata(session->SessionPrivateData(), buffer, mediaType);
               result = ERROR_NONE;
           }

            //Clean up
            if(subSampleInfoPtr != nullptr) {
               free(subSampleInfoPtr);
            }

            gst_buffer_unmap(buffer, &dataMap);

            if (subSample != nullptr) {
              gst_buffer_unmap(subSample, &sampleMap);
            }

            if (IV != nullptr) {
               gst_buffer_unmap(IV, &ivMap);
            }

            if (keyID != nullptr) {
              gst_buffer_unmap(keyID, &keyIDMap);
            }
        } else {
            TRACE_L1("opencdm_gstreamer_session_decrypt_buffer: Missing Protection Metadata.");
            result = ERROR_INVALID_DECRYPT_BUFFER;
        }

    }

exit:
    return (result);
}

// ============================================================================
// RDKDEV-1281: Multi-frame decrypt implementation.
//
// Everything below this point is a self-contained addition for
// opencdm_gstreamer_session_decrypt_buffer_multi_once(). It does not modify,
// call into, or get called by opencdm_gstreamer_session_decrypt_buffer_once()
// (single buffer decrypt) above - the two implementations are kept
// independent on purpose so existing single-decrypt behaviour/tests are
// unaffected by this addition.
// ============================================================================
#ifdef ENABLE_MULTI_DECRYPT

namespace {

    bool mapBuffer(GstBuffer *buffer, GstMapFlags flags, GstMapInfo *map, uint8_t **data, uint32_t *size)
    {
        bool ret{false};
        if (gst_buffer_map (buffer, map, flags)) {
            *data = reinterpret_cast<uint8_t* >(map->data);
            *size = static_cast<uint32_t >(map->size);
            ret = true;
        }
        return ret;
    }

    struct ProtectionMetaInfo
    {
        gboolean encrypted{};
        uint8_t *dataBuf{nullptr};
        uint32_t dataSize{};
        GstMapInfo dataBufMap{};
        uint8_t *ivBuf{nullptr};
        uint32_t ivSize{};
        GstMapInfo ivBufMap{};
        GstBuffer *ivGstBuf{nullptr};
        uint8_t *keyIdBuf{nullptr};
        uint32_t keyIdSize{};
        GstMapInfo keyIdBufMap{};
        GstBuffer *keyIdGstBuf{nullptr};
        uint8_t *subSamplesBuf{nullptr};
        uint32_t subSamplesSize{};
        GstMapInfo subSamplesBufMap{};
        GstBuffer *subSamplesGstBuf{nullptr};
        uint32_t subSamplesCount{};
        EncryptionPattern pattern{0, 0};
        EncryptionScheme encScheme{EncryptionScheme::Clear};
    };

    OpenCDMError extractProtectionMetaMulti(std::vector<GstBuffer*> const& vbuff, std::vector<ProtectionMetaInfo>& metaInfo)
    {
        OpenCDMError result{ERROR_NONE};
        const GValue* value{nullptr};

        ASSERT(vbuff.size() == metaInfo.size());

        for (size_t vBuffIdx = 0; vBuffIdx < vbuff.size(); ++vBuffIdx) {
            GstProtectionMeta* protectionMeta = gst_buffer_get_protection_meta(vbuff[vBuffIdx]);
            if (protectionMeta) {
                if (!gst_structure_get_uint(protectionMeta->info, "subsample_count", &metaInfo[vBuffIdx].subSamplesCount)) {
                    TRACE_L1("Missing subsample count in protectionMeta");
                }
                if (metaInfo[vBuffIdx].subSamplesCount) {
                    value = gst_structure_get_value(protectionMeta->info, "subsamples");
                    if (value) {
                        metaInfo[vBuffIdx].subSamplesGstBuf = gst_value_get_buffer(value);
                        if (metaInfo[vBuffIdx].subSamplesGstBuf && (mapBuffer(metaInfo[vBuffIdx].subSamplesGstBuf, GST_MAP_READ,
                                &metaInfo[vBuffIdx].subSamplesBufMap, &metaInfo[vBuffIdx].subSamplesBuf, &metaInfo[vBuffIdx].subSamplesSize) == false)) {

                            TRACE_L1("Invalid subsamples buffer");
                            result = ERROR_INVALID_DECRYPT_BUFFER;
                            break;
                        }
                    } else {
                        TRACE_L1("Missing subsamples buffer");
                        result = ERROR_INVALID_DECRYPT_BUFFER;
                        break;
                    }
                }

                value = gst_structure_get_value(protectionMeta->info, "iv");
                if (value) {
                    metaInfo[vBuffIdx].ivGstBuf = gst_value_get_buffer(value);
                    if(metaInfo[vBuffIdx].ivGstBuf && (mapBuffer(metaInfo[vBuffIdx].ivGstBuf, GST_MAP_READ, &metaInfo[vBuffIdx].ivBufMap,
                            &metaInfo[vBuffIdx].ivBuf, &metaInfo[vBuffIdx].ivSize) == false)) {
                        TRACE_L1("Invalid IV buffer");
                        result = ERROR_INVALID_DECRYPT_BUFFER;
                        break;
                    }
                } else {
                    TRACE_L1("Missing IV buffer");
                    result = ERROR_INVALID_DECRYPT_BUFFER;
                    break;
                }

                unsigned initWithLast15 = 0;
                if (!gst_structure_get_uint(protectionMeta->info, "initWithLast15", &initWithLast15)) {
                    TRACE_L3("Missing initWithLast15 value.");
                }
                if (initWithLast15 == 1) {
                    // Reuses the same byte-swap helper as the single-buffer flow above.
                    swapIVBytes(metaInfo[vBuffIdx].ivBuf, metaInfo[vBuffIdx].ivSize);
                }

                if(mapBuffer(vbuff[vBuffIdx], GST_MAP_READWRITE, &metaInfo[vBuffIdx].dataBufMap, &metaInfo[vBuffIdx].dataBuf, &metaInfo[vBuffIdx].dataSize) == false) {
                    TRACE_L1("Invalid buffer");
                    result = ERROR_INVALID_DECRYPT_BUFFER;
                    break;
                }

                value = gst_structure_get_value(protectionMeta->info, "kid");
                if (value) {
                    metaInfo[vBuffIdx].keyIdGstBuf = gst_value_get_buffer(value);
                    if(metaInfo[vBuffIdx].keyIdGstBuf && (mapBuffer(metaInfo[vBuffIdx].keyIdGstBuf, GST_MAP_READ, &metaInfo[vBuffIdx].keyIdBufMap,
                            &metaInfo[vBuffIdx].keyIdBuf, &metaInfo[vBuffIdx].keyIdSize) == false)) {
                        TRACE_L1("Invalid key id buffer");
                        result = ERROR_INVALID_DECRYPT_BUFFER;
                        break;
                    }
                } else {
                    TRACE_L1("Missing key id buffer");
                    result = ERROR_INVALID_DECRYPT_BUFFER;
                    break;
                }

                //Get Enc Scheme and Pattern
                metaInfo[vBuffIdx].encScheme = AesCtr_Cenc;
                if (gst_structure_has_name(protectionMeta->info, "application/x-cbcs")) {
                    metaInfo[vBuffIdx].encScheme = AesCbc_Cbcs;
                } else {
                    const char* cipherModeBuf = gst_structure_get_string(protectionMeta->info, "cipher-mode");
                    if(g_strcmp0(cipherModeBuf, "cbcs") == 0) {
                        metaInfo[vBuffIdx].encScheme = AesCbc_Cbcs;
                    }
                }
                gst_structure_get_uint(protectionMeta->info, "crypt_byte_block", &metaInfo[vBuffIdx].pattern.encrypted_blocks);
                gst_structure_get_uint(protectionMeta->info, "skip_byte_block", &metaInfo[vBuffIdx].pattern.clear_blocks);
            } else {
                TRACE_L1("Missing Protection Metadata");
            }
        }

        return result;
    }

    void extractMediaInfoMulti(const GstCaps* caps, MediaProperties &streamProperties, bool &isSecureMemoryDisabled)
    {
        //Get Stream Properties from GstCaps
        gchar *capsStr = gst_caps_to_string (caps);
        if (capsStr != nullptr) {
            WPEFramework::Plugin::CapsParser capsParser;
            capsParser.Parse(reinterpret_cast<const uint8_t*>(capsStr), strlen(capsStr));
            streamProperties.height = capsParser.GetHeight();
            streamProperties.width = capsParser.GetWidth();
            switch (capsParser.GetMediaType()) {
                case CDMi::MediaType::Video:
                    streamProperties.media_type = MediaType_Video;
                    if (capsParser.IsSecureMemoryDisabled()) {
                        isSecureMemoryDisabled = true;
                        TRACE_L1("Secure Memory Preallocation disabled as decrypt-to-host is set\n");
                    }
                    break;

                case CDMi::MediaType::Audio:
                    streamProperties.media_type = MediaType_Audio;
                    break;

                case CDMi::MediaType::Data:
                    streamProperties.media_type = MediaType_Data;
                    break;

                default:
                    streamProperties.media_type = MediaType_Unknown;
                    break;
            }

            g_free(capsStr);
        } else {
            TRACE_L1("Could not convert caps to string");
        }
    }

    media_type toMediaTypeMulti(const MediaProperties &streamProperties)
    {
        media_type mediaType {Unknown};
        switch(streamProperties.media_type) {
            case MediaType_Video:
                mediaType = Video;
                break;
            case MediaType_Audio:
                mediaType = Audio;
                break;
            case MediaType_Data:
                mediaType = Data;
                break;
            case MediaType_Unknown:
                mediaType = Unknown;
                break;
        }
        return mediaType;
    }

    std::string toStringMulti(const media_type &mediaType)
    {
        std::string typeStr;
        switch(mediaType) {
            case Video:
                typeStr = "Video";
                break;
            case Audio:
                typeStr = "Audio";
                break;
            case Data:
                typeStr = "Data";
                break;
            case Unknown:
                typeStr = "Unknown";
                break;
        }
        return typeStr;
    }

    bool keyIdsEqual(uint8_t *lhsBuf, uint32_t lhsSize, uint8_t *rhsBuf, uint32_t rhsSize)
    {
        bool equal = true;
        if ( (lhsSize != rhsSize) || (lhsBuf == nullptr) || (rhsBuf == nullptr)) {
            equal = false;
        } else {
            equal = (memcmp(lhsBuf, rhsBuf, lhsSize) == 0);
        }
        return equal;
    }

} // namespace

void extend_subsample_map_multi(std::vector<SubSampleInfo> &subSampleVector, uint32_t frameSize, uint32_t totalSubsampleBytes)
{
    ASSERT(frameSize > totalSubsampleBytes);

    RDKPerf perf_subsample(__FUNCTION__);

    // Add an extra subsample(s) entry to account for the size mismatch
    uint32_t additionalBytes = frameSize - totalSubsampleBytes;
    // Calculate how many extra subsamples are needed to fit 16bit clear data size
    while (additionalBytes > 0) {
        uint16_t clearBytes = 0;
        if (additionalBytes > 0xFFFF) {
            clearBytes = 0xFFFF;
        } else {
            clearBytes = static_cast<uint16_t>(additionalBytes);
        }
        subSampleVector.emplace_back(SubSampleInfo{clearBytes, 0});
        additionalBytes -= clearBytes;
    }
}

OpenCDMError validate_subsample_map_multi(std::vector<SubSampleInfo> &subSampleVector, uint32_t frameSize, uint32_t totalSubsampleBytes)
{
    OpenCDMError retVal = ERROR_NONE;

    if(frameSize == totalSubsampleBytes) {
        // Perfect match, no need to adjust anything
    }
    else if(frameSize > totalSubsampleBytes) {
        TRACE_L3("Subsample mapping size mismatch. FrameSize: %u, TotalBytes from SubsampleInfo: %u", frameSize, totalSubsampleBytes);
        extend_subsample_map_multi(subSampleVector, frameSize, totalSubsampleBytes);
    }
    else if(frameSize < totalSubsampleBytes) {
        TRACE_L1("Subsample mapping size exceeds data size. FrameSize: %u, TotalBytes from SubsampleInfo: %u", frameSize, totalSubsampleBytes);
        retVal = ERROR_INVALID_DECRYPT_BUFFER;
    }

    return retVal;
}

// Decrypts a vector of GstBuffer-s (all sharing the same KID) in a single call. This is the
// multi-frame counterpart of opencdm_gstreamer_session_decrypt_buffer_once() above, implemented
// independently so the single-buffer path is not affected by this feature.
OpenCDMError opencdm_gstreamer_session_decrypt_buffer_multi_once(struct OpenCDMSession* session, const std::vector<GstBuffer*> &vbuff, GstCaps* caps)
{
    OpenCDMError result{ERROR_NONE};

    if (session != nullptr) {

        std::vector<ProtectionMetaInfo> vProtectionInfo(vbuff.size());
        result = extractProtectionMetaMulti(vbuff, vProtectionInfo);

        if (result == ERROR_NONE) {
            std::vector<SampleInfo> vSampleInfo;
            std::vector<std::vector<SubSampleInfo>> vSubSampleInfo(vbuff.size());
            std::vector<GstBuffer*> vbuffToDecrypt;
            uint32_t totalBytesToDecrypt{};

            for (size_t vBuffIdx = 0; vBuffIdx < vbuff.size(); ++vBuffIdx) {

                //====================================================
                //Check if there is anything to decrypt in the sample

                if (vProtectionInfo[vBuffIdx].subSamplesGstBuf == nullptr && vProtectionInfo[vBuffIdx].ivGstBuf == nullptr &&
                        vProtectionInfo[vBuffIdx].keyIdGstBuf == nullptr) {
                    TRACE_L1("Nothing to decrypt in sample id: %zu", vBuffIdx);
                    continue;
                } else {
                    if (vProtectionInfo[vBuffIdx].subSamplesBuf) {
                        uint32_t encryptedSubSampleCount{};
                        GstByteReader* reader = gst_byte_reader_new(vProtectionInfo[vBuffIdx].subSamplesBuf, vProtectionInfo[vBuffIdx].subSamplesSize);
                        uint16_t inClear = 0;
                        uint32_t inEncrypted = 0;
                        for (uint32_t index = 0; index < vProtectionInfo[vBuffIdx].subSamplesCount; index++) {
                            gst_byte_reader_get_uint16_be(reader, &inClear);
                            gst_byte_reader_get_uint32_be(reader, &inEncrypted);
                            if (inEncrypted) {
                                encryptedSubSampleCount++;
                                break;
                            }
                        }
                        gst_byte_reader_free(reader);
                        if (encryptedSubSampleCount == 0) {
                            TRACE_L1("Nothing to decrypt in subSamples, sample id: %zu", vBuffIdx);
                            continue;
                        }
                    }
                    if (vProtectionInfo[vBuffIdx].dataSize == 0) {
                        TRACE_L1("Nothing to decrypt - empty buffer, sample id: %zu", vBuffIdx);
                        continue;
                    }
                }

                //====================================================
                //Prepare SampleInfo
                if (vProtectionInfo[vBuffIdx].subSamplesBuf) {
                    GstByteReader* reader = gst_byte_reader_new(vProtectionInfo[vBuffIdx].subSamplesBuf, vProtectionInfo[vBuffIdx].subSamplesSize);
                    uint16_t inClear = 0;
                    uint32_t inEncrypted = 0;
                    uint32_t totalSubSampleBytes = 0;
                    for (uint32_t index = 0; index < vProtectionInfo[vBuffIdx].subSamplesCount; index++) {
                        gst_byte_reader_get_uint16_be(reader, &inClear);
                        gst_byte_reader_get_uint32_be(reader, &inEncrypted);

                        vSubSampleInfo[vBuffIdx].emplace_back(SubSampleInfo{inClear, inEncrypted});
                        totalSubSampleBytes += inClear + inEncrypted;
                    }
                    gst_byte_reader_free(reader);
                    result = validate_subsample_map_multi(vSubSampleInfo[vBuffIdx], vProtectionInfo[vBuffIdx].dataSize, totalSubSampleBytes);
                    if(result != ERROR_NONE) {
                        break;
                    }
                } else {
                    uint16_t inClear = 0;
                    uint32_t inEncrypted = vProtectionInfo[vBuffIdx].dataSize;
                    vSubSampleInfo[vBuffIdx].emplace_back(SubSampleInfo{inClear, inEncrypted});
                }

                if (!keyIdsEqual(vProtectionInfo[0].keyIdBuf, vProtectionInfo[0].keyIdSize,
                        vProtectionInfo[vBuffIdx].keyIdBuf, vProtectionInfo[vBuffIdx].keyIdSize)) {
                    TRACE_L1("Key id needs to be same for all GstBuffers");
                    result = ERROR_INVALID_DECRYPT_BUFFER;
                    break;
                }

                if (vSubSampleInfo[vBuffIdx].size() > 255) {
                    TRACE_L1("Max number of sub samples exceeded %zu", vSubSampleInfo[vBuffIdx].size());
                    result = ERROR_INVALID_DECRYPT_BUFFER;
                    break;
                }
                vSampleInfo.emplace_back(SampleInfo{vProtectionInfo[vBuffIdx].encScheme, vProtectionInfo[vBuffIdx].pattern,
                    vProtectionInfo[vBuffIdx].ivBuf, static_cast<uint8_t>(vProtectionInfo[vBuffIdx].ivSize),
                    vProtectionInfo[vBuffIdx].keyIdBuf, static_cast<uint8_t>(vProtectionInfo[vBuffIdx].keyIdSize),
                    static_cast<uint8_t>(vSubSampleInfo[vBuffIdx].size()), vSubSampleInfo[vBuffIdx].data()});

                totalBytesToDecrypt += vProtectionInfo[vBuffIdx].dataSize;
                vProtectionInfo[vBuffIdx].encrypted = true;

                vbuffToDecrypt.push_back(vbuff[vBuffIdx]);
            }//for vbuff

            std::string perfString(__FUNCTION__);
            //Get Stream Properties from GstCaps
            MediaProperties streamProperties{};
            media_type mediaType = Data;
            bool isSecureMemoryDisabled = false;
            if(caps != nullptr){
                extractMediaInfoMulti(caps, streamProperties, isSecureMemoryDisabled);
                mediaType = toMediaTypeMulti(streamProperties);
                if (totalBytesToDecrypt == 0) {
                    perfString += "_clearData";
                } else if (streamProperties.media_type == MediaType_Unknown) {
                    perfString += "_NoGstCaps";
                } else {
                    perfString += "_" + toStringMulti(mediaType);
                }
            }
            RDKPerf perf(perfString.c_str());

            if (s_svpSetValueFn && session->NeedsDecryptToHostUpdate(isSecureMemoryDisabled)) {
                const gboolean decryptToHost = isSecureMemoryDisabled ? TRUE : FALSE;
                if (!s_svpSetValueFn(session->SessionPrivateData(),
                                "decryptToHost",
                                (void*)&decryptToHost,
                                sizeof(decryptToHost))) {
                    TRACE_L1("Failed to set decryptToHost=%s in SVP context\n", decryptToHost ? "true" : "false");
                }
            }

            if((totalBytesToDecrypt > 0) && (result == ERROR_NONE)) {
               uint8_t* svpData;
               const gboolean needSecureMemoryPrealloc = (streamProperties.media_type == MediaType_Video)
                                                       && gst_svp_context_supports_memory_prealloc(session->SessionPrivateData())
                                                       && (!isSecureMemoryDisabled);
               uint32_t dataBlockSize = gst_svp_allocate_data_block(session->SessionPrivateData(),
                                                                    (void**) &svpData,
                                                                    totalBytesToDecrypt,
                                                                    totalBytesToDecrypt,
                                                                    needSecureMemoryPrealloc);

               if (dataBlockSize) {
                   uint8_t* encryptedData = reinterpret_cast<uint8_t *>(gst_svp_header_get_start_of_data(session->SessionPrivateData(), svpData));
                   uint8_t* encryptedDataIter = encryptedData;

                   for (size_t vBuffIdx = 0; vBuffIdx < vbuff.size(); ++vBuffIdx) {
                       if (vProtectionInfo[vBuffIdx].encrypted) {
                           memcpy(encryptedDataIter, vProtectionInfo[vBuffIdx].dataBuf, vProtectionInfo[vBuffIdx].dataSize);
                           encryptedDataIter += vProtectionInfo[vBuffIdx].dataSize;
                       }
                   }

                   TokenType tokenType = TokenType::InPlace;
                   if (!gst_svp_header_get_field(session->SessionPrivateData(), svpData, SvpHeaderFieldName::Type, (uint32_t*) &tokenType)) {
                       TRACE_L1("Failed to get type from SVP header");
                   }

                   const bool isRevokedAllocation = needSecureMemoryPrealloc
                                                    && tokenType != TokenType::InPlace
                                                    && tokenType != TokenType::Handle
                                                    && tokenType != TokenType::PreAllocatedHandle;

                   if (!isRevokedAllocation) {
                       if(isSecureMemoryDisabled)
                       {
                          TRACE_L1("Secure Memory Preallocation disabled, Setting TokenType to InPlace");
                          gst_svp_header_set_field(session->SessionPrivateData(), svpData, SvpHeaderFieldName::Type, (uint32_t)TokenType::InPlace);
                       }
                       GstPerf* ocdm_perf = new GstPerf("opencdm_session_decrypt_v3");
                       result = opencdm_session_decrypt_v3(session,
                                                           svpData,
                                                           dataBlockSize,
                                                           vSampleInfo.data(),
                                                           vSampleInfo.size(),
                                                           &streamProperties);
                       delete ocdm_perf;
                   } else {
                       TRACE_L1("Skipping decrypt as resources have been revoked");
                       result = ERROR_NONE;
                   }

                   if(result == ERROR_NONE) {
                       GstPerf* svpTransform_perf3 = new GstPerf("opencdm_svp_transform_subsample");
                       if (!gst_buffer_vector_append_svp_transform(session->SessionPrivateData(), vbuffToDecrypt, svpData, totalBytesToDecrypt)) {
                           result = ERROR_FAIL;
                       }
                       delete svpTransform_perf3;
                   }
                   gst_svp_free_data_block(session->SessionPrivateData(), svpData);
               } else {
                   result = ERROR_OUT_OF_MEMORY;
                   TRACE_L1("Failed to allocate svp data block");
               }
            }

            if (result == ERROR_NONE) {
                for (size_t vBuffIdx = 0; vBuffIdx < vbuff.size(); ++vBuffIdx) {
                    if (vProtectionInfo[vBuffIdx].encrypted == false) {
                        gst_buffer_svp_transform_from_cleardata(session->SessionPrivateData(), vbuff[vBuffIdx], mediaType);
                    }
                }
            }
        } //if protection meta valid

        for (size_t vBuffIdx = 0; vBuffIdx < vbuff.size(); ++vBuffIdx) {
            if (vProtectionInfo[vBuffIdx].dataBufMap.data) {
                gst_buffer_unmap(vbuff[vBuffIdx], &vProtectionInfo[vBuffIdx].dataBufMap);
            }
            if (vProtectionInfo[vBuffIdx].ivBufMap.data) {
                gst_buffer_unmap(vProtectionInfo[vBuffIdx].ivGstBuf, &vProtectionInfo[vBuffIdx].ivBufMap);
            }
            if (vProtectionInfo[vBuffIdx].keyIdBufMap.data) {
                gst_buffer_unmap(vProtectionInfo[vBuffIdx].keyIdGstBuf, &vProtectionInfo[vBuffIdx].keyIdBufMap);
            }
            if (vProtectionInfo[vBuffIdx].subSamplesBufMap.data) {
                gst_buffer_unmap(vProtectionInfo[vBuffIdx].subSamplesGstBuf, &vProtectionInfo[vBuffIdx].subSamplesBufMap);
            }
        }
    } else {
        result = ERROR_INVALID_SESSION;
        TRACE_L1("Invalid session argument");
    }

    return result;
}
#endif // ENABLE_MULTI_DECRYPT
