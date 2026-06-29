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

#include "CapsParser.h"

static constexpr TCHAR StartingCharacter = ')';
static constexpr TCHAR EndingCharacter   = ',';
static constexpr TCHAR WidthTag[]        = _T("width");
static constexpr TCHAR HeightTag[]       = _T("height");
static constexpr TCHAR MediaTag[]        = _T("original-media-type");

namespace WPEFramework {
    namespace Plugin {

        CapsParser::CapsParser() 
            : _lastHash(0)
            , _mediaType(CDMi::Unknown)
            , _width(0)
            , _height(0) {
        }

        CapsParser::~CapsParser() {
        }

        void CapsParser::Parse(const uint8_t* info, const uint16_t infoLength) /* override */ 
        {
            if(infoLength > 0) {
                std::string infoStr(reinterpret_cast<const char*>(info), infoLength);

                std::hash<::string> hash_fn;
                size_t info_hash = hash_fn(infoStr);
                if(_lastHash != info_hash) {
                    _lastHash = info_hash;

                    // Parse the data
                    std::string result = FindMarker(infoStr, MediaTag);
                    if(!result.empty()) {
                        if(result.find("video") != ::string::npos) {
                            _mediaType = CDMi::Video;
                        }
                        else if(result.find("audio") != ::string::npos) {
                            _mediaType = CDMi::Audio;
                        }
                        else {
                            TRACE(Trace::Error, (Core::Format(_T("Found and unknown media type %s\n"), result.c_str())));
                            _mediaType = CDMi::Unknown;
                        }
                    }
                    else {
			if(strcasestr((const char*)info, "audio") != NULL) {
                            _mediaType = CDMi::Audio;
                        }
			else if(strcasestr((const char*)info, "video") != NULL) {
                            _mediaType = CDMi::Video;
                        }
                        else
                        {
                            TRACE(Trace::Warning, (_T("No result for media type")));
                        }
                    }

                    if(_mediaType == CDMi::Video) {

                        result = FindMarker(infoStr, WidthTag);

                        if (result.length() > 0) {
                            _width = Core::NumberType<uint16_t>(result.c_str(), result.length(), NumberBase::BASE_DECIMAL);
                        }
                        else {
                            _width = 0;
                            TRACE(Trace::Warning, (_T("No result for width")));
                        }

                        result = FindMarker(infoStr, HeightTag);

                        if (result.length() > 0) {
                            _height = Core::NumberType<uint16_t>(result.c_str(), result.length(), NumberBase::BASE_DECIMAL);
                        }
                        else {
                            _height = 0;
                            TRACE(Trace::Warning, (_T("No result for height")));
                        }
                        result = FindDecryptToHost(infoStr);
                        bool decryptToHost = false;
                        if (!result.empty()) {
                            const char c = result[0];
                            if ((c == '1') || (c == 't') || (c == 'T')) {
                                decryptToHost = true;
                                TRACE(Trace::Information, (_T("decrypt-to-host is set to true in caps")));
                            }
                        }
                        else {
                            TRACE(Trace::Warning, (_T("No result for decrypt-to-host")));
                        }

                        _no_secure_memory = decryptToHost;
                        if (_no_secure_memory) {
                            TRACE(Trace::Information, (Core::Format(_T("decrypt-to-host is set to true, width %d height %d _no_secure_memory %d"), _width, _height,_no_secure_memory)));
                        } else {
                            _no_secure_memory = false;
                            TRACE(Trace::Information, (Core::Format(_T("decrypt-to-host is not set, width %d height %d _no_secure_memory %d"), _width, _height,_no_secure_memory)));
                        }
                    }
                    else {
                        // Audio
                        _width  = 0;
                        _height = 0;
                        _no_secure_memory = false;
                    }
                }
            }
        }

        std::string CapsParser::FindMarker(const std::string& data, const TCHAR tag[]) const
        {
            std::string retVal;

            size_t found = data.find(tag);
            TRACE(Trace::Information, (Core::Format(_T("Found tag <%s> in <%s> at location %d"), tag, data.c_str(), found)));
            if(found != ::string::npos) {
                // Found the marker
                // Find the end of the gst caps type identifier
                size_t start = data.find(StartingCharacter, found) + 1;  // step over the ")"
                size_t end = data.find(EndingCharacter, start);
                if(end == ::string::npos) {
                    // Went past the end of the string
                    end = data.length();
                }
                retVal = data.substr(start, end - start);
                TRACE(Trace::Information, (Core::Format(_T("Found substr <%s>"), retVal.c_str())));
            }
            return retVal;
        }
        std::string CapsParser::FindDecryptToHost(const std::string& data) const
        {
            std::string retVal;

            // String Format expected: "decrypt-to-host=(boolean)true"

            size_t found = data.find("decrypt-to-host=");
            TRACE(Trace::Information, (Core::Format(_T("Found tag <%s> in <%s> at location %d"), "decrypt-to-host=", data.c_str(), found)));
            if(found != ::string::npos) {
                // Found the marker
                // Find the start of the boolean value after "(boolean)"
                size_t start = data.find("(boolean)", found);
                if(start != ::string::npos) {
                    start += 9; // step over "(boolean)"
                } else {
                    start = data.find('=', found);
                    if (start == ::string::npos) {
                        return retVal;
                    }
                    start += 1;
                }

                while (start < data.length() && data[start] == ' ') {
                    ++start;
                }

                    size_t end = data.find(EndingCharacter, start);
                    if(end == ::string::npos) {
                        // Went past the end of the string
                        end = data.length();
                    }

                    std::string boolStr = data.substr(start, end - start);
                while (!boolStr.empty() && boolStr.back() == ' ') {
                    boolStr.pop_back();
                }

                TRACE(Trace::Information, (Core::Format(_T("Found decrypt-to-host value <%s>"), boolStr.c_str())));
                retVal = boolStr;
            }
            return retVal;
        }
    }
}
