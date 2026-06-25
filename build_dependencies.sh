#!/bin/bash
#
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2026 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -x
set -e
##############################
GITHUB_WORKSPACE="${PWD}"
ls -la ${GITHUB_WORKSPACE}
cd ${GITHUB_WORKSPACE}

# # ############################# 
#1. Install Dependencies and packages

apt update
apt install -y valgrind lcov clang libsystemd-dev meson curl libunwind-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
pip install jsonref

############################
# Build trevor-base64
if [ ! -d "trower-base64" ]; then
git clone https://github.com/xmidt-org/trower-base64.git
fi
cd trower-base64
meson setup --warnlevel 3 --werror build
ninja -C build
ninja -C build install
cd ..
###########################################
# Clone the required repositories


git clone --branch R4.4.3 https://github.com/rdkcentral/ThunderTools.git

git clone --branch R4.4.2 https://github.com/rdkcentral/Thunder.git

git clone --branch R4_4 https://github.com/rdkcentral/ThunderClientLibraries.git

git clone --branch main https://github.com/rdkcentral/entservices-apis.git

git clone --branch 1.0.14 https://github.com/rdkcentral/entservices-testframework.git

############################
# Patch ThunderClientLibraries for IOCDM interface compatibility
echo "======================================================================================"
echo "patching ThunderClientLibraries open_cdm_impl.h"
cd ThunderClientLibraries
#if ! grep -q "GetSupportedRobustness(const string& keySystem" Source/ocdm/open_cdm_impl.h; then
#target_file="Source/ocdm/open_cdm_impl.h"
#tmp_file=$(mktemp)
#
#awk '
#BEGIN { inserted = 0 }
#/^[[:space:]]*\/\/ Create a MediaKeySession using the supplied init data and CDM data\./ && inserted == 0 {
#    print "    Exchange::OCDM_RESULT GetSupportedRobustness(const string& keySystem, RPC::IStringIterator*& robustness) const override"
#    print "    {"
#    print "        Exchange::OCDM_RESULT result = Exchange::OCDM_INVALID_ACCESSOR;"
#    print "        robustness = nullptr;"
#    print ""
#    print "        if (_remote != nullptr) {"
#    print "            result = _remote->GetSupportedRobustness(keySystem, robustness);"
#    print "        }"
#    print ""
#    print "        return (result);"
#    print "    }"
#    print ""
#    inserted = 1
#}
#{ print }
#END {
#    if (inserted == 0) {
#        exit 1
#    }
#}
#' "$target_file" > "$tmp_file"
#
#mv "$tmp_file" "$target_file"
#fi
#
#if ! grep -q "GetSupportedRobustness(const string& keySystem" Source/ocdm/open_cdm_impl.h; then
#    echo "Failed to patch ThunderClientLibraries: GetSupportedRobustness not found"
#    exit 1
#fi
cd -

############################
# Build Thunder-Tools
echo "======================================================================================"
echo "buliding thunderTools"
cd ThunderTools
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/00010-R4.4-Add-support-for-project-dir.patch
cd -


cmake -G Ninja -S ThunderTools -B build/ThunderTools \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr"

cmake --build build/ThunderTools --target install


############################
# Build Thunder
echo "======================================================================================"
echo "buliding thunder"

cd Thunder
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/Use_Legact_Alt_Based_On_ThunderTools_R4.4.3.patch
#patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/error_code_R4_4.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/1004-Add-support-for-project-dir.patch
patch -p1 < $GITHUB_WORKSPACE/entservices-testframework/patches/RDKEMW-733-Add-ENTOS-IDS.patch
cd -

cmake -G Ninja -S Thunder -B build/Thunder \
    -DMESSAGING=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DGENERIC_CMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr" \
    -DBUILD_TYPE=Debug \
    -DBINDING=127.0.0.1 \
    -DPORT=55555 \
    -DEXCEPTIONS_ENABLE=ON

cmake --build build/Thunder --target install

############################
# Build entservices-apis
echo "======================================================================================"
echo "buliding entservices-apis"
cd entservices-apis
rm -rf jsonrpc/DTV.json
cd ..

cmake -G Ninja -S entservices-apis  -B build/entservices-apis \
    -DEXCEPTIONS_ENABLE=ON \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr"

cmake --build build/entservices-apis --target install


############################
# Build ThunderClientLibraries (OCDM)
echo "======================================================================================"
echo "building ThunderClientLibraries"

cmake -G Ninja -S ThunderClientLibraries -B build/ThunderClientLibraries \
    -DCDMI=ON \
    -DCDMI_ADAPTER_IMPLEMENTATION=gstreamer \
    -DCMAKE_INSTALL_PREFIX="$GITHUB_WORKSPACE/install/usr" \
    -DCMAKE_MODULE_PATH="$GITHUB_WORKSPACE/install/tools/cmake" \
    -DCMAKE_PREFIX_PATH="$GITHUB_WORKSPACE/install/usr"
cmake --build build/ThunderClientLibraries --target install

ls -la ${GITHUB_WORKSPACE}
