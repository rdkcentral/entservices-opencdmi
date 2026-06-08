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

#include <gst_svp_meta.h>
#include "../CapsParser.h"

typedef gboolean (*svp_set_value_fn_t)(void *, const char *, void *, const size_t);
static svp_set_value_fn_t s_svpSetValueFn = nullptr;

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

OpenCDMError opencdm_gstreamer_session_decrypt(struct OpenCDMSession* session, GstBuffer* buffer, GstBuffer* subSample, const uint32_t subSampleCount,
                                                GstBuffer* IV, GstBuffer* keyID, uint32_t initWithLast15)
{
    OpenCDMError result (ERROR_INVALID_SESSION);

    if (session != nullptr) {
        GstMapInfo dataMap;
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

        GstMapInfo ivMap;
        if (gst_buffer_map(IV, &ivMap, (GstMapFlags) GST_MAP_READ) == false) {
            gst_buffer_unmap(buffer, &dataMap);
            fprintf(stderr, "Invalid IV buffer.\n");
            return (ERROR_INVALID_DECRYPT_BUFFER);
        }

        GstMapInfo keyIDMap;
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
            GstMapInfo sampleMap;

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

OpenCDMError opencdm_gstreamer_session_decrypt_buffer(struct OpenCDMSession* session, GstBuffer* buffer, GstCaps* caps) {

    OpenCDMError result (ERROR_INVALID_SESSION);

    if (session != nullptr) {

        GstMapInfo dataMap;
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
            GstMapInfo sampleMap;
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
            GstMapInfo ivMap;
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
            GstMapInfo keyIDMap;
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

            if (s_svpSetValueFn) {
                const gboolean decryptToHost = capsParser.IsSecureMemoryDisabled() ? TRUE : FALSE;
                if (!s_svpSetValueFn(session->SessionPrivateData(),
                                "decryptToHost",
                                (void*)&decryptToHost,
                                sizeof(decryptToHost))) {
                    TRACE_L1("Failed to set decryptToHost=%s in SVP context\n", decryptToHost ? "true" : "false");
                } else {
                    TRACE_L1("Sucess to set decryptToHost=%s in SVP context\n", decryptToHost ? "true" : "false");
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
