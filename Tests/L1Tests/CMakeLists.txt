# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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

cmake_minimum_required(VERSION 3.8)
project(OpenCDMIL1Tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
enable_testing()

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(GTest REQUIRED)

set (TEST_SRC
    tests/ClientCapsParserL1Tests.cpp
    tests/ClientOpenCdmExtL1Tests.cpp
    tests/PluginOcdmJsonRpcInterfaceL1Tests.cpp
    tests/PluginCencParserL1Tests.cpp
    tests/mocks/ClientModule.cpp
    tests/mocks/FakeServerInterfacesCommon.cpp
    tests/mocks/FakeServerInterfacesStandaloneAccessor.cpp
    ../../client/CapsParser.cpp
    ../../client/open_cdm_ext.cpp
    ../../plugin/CENCParser.cpp
    ../../plugin/Module.cpp
    ../../plugin/OCDM.cpp
    ../../plugin/OCDMJsonRpc.cpp
)

set (TEST_LIB
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

set (TEST_INC
    ../../helpers
    ../../client
    ../../plugin
    tests/mocks
)

#########################################################################################
# add_plugin_test_ex: Macro to add plugin tests, it will append to TEST_SRC, TEST_INC,
#                     and TEST_LIB. Args are positional.
#                     arg1: test file list as **string**
#                     arg2: include dir list as **string**
#                     arg3: plugin libs to link list as **string**
#
# sample invocation : 
#    add_plugin_test_ex(PLUGIN_NAME
#                       "test/file1.cpp;test/file2.cpp"
#                       "../../inc_dir1;../../inc_dir2"
#                       "${NAMESPACE}PluginName;${NAMESPACE}PluginNameImplementation")
# ----------------------------------------- OR ------------------------------------------
# list(APPEND PLUGIN_NAME_SRC test/file1.cpp)
# list(APPEND PLUGIN_NAME_SRC test/file2.cpp)
#
# list(APPEND PLUGIN_NAME_INC ../../inc_dir1)
# list(APPEND PLUGIN_NAME_INC ../../inc_dir2)
#
# list(APPEND PLUGIN_NAME_LIB ${NAMESPACE}PluginName)
# list(APPEND PLUGIN_NAME_LIB ${NAMESPACE}PluginNameImplementation)
#
#    add_plugin_test_ex(PLUGIN_NAME
#                       "${PLUGIN_NAME_SRC}"
#                       "${PLUGIN_NAME_INC}"
#                       "${PLUGIN_NAME_LIB}")
#
# NOTE: Alternatively test can choose to update `TEST_SRC`, `TEST_INC` & `TEST_LIB`
#       directly (like in the case of Miracast)
#########################################################################################
macro(add_plugin_test_ex plugin_opt plugin_test_sources_str plugin_includes_str plugin_libs_str)
    # Check if the plugin option is enabled
    if(${plugin_opt})
        message(STATUS "${plugin_opt}=ON")

        string(REPLACE ";" ";" srclist "${plugin_test_sources_str}")
        string(REPLACE ";" ";" inclist "${plugin_includes_str}")
        string(REPLACE ";" ";" liblist "${plugin_libs_str}")

        foreach(item IN LISTS srclist)
            # Add each test source file
            list(APPEND TEST_SRC ${item})
        endforeach()

        foreach(item IN LISTS inclist)
            # Add each include directory
            list(APPEND TEST_INC ${item})
        endforeach()

        foreach(item IN LISTS liblist)
            # Add each libraries to link
            list(APPEND TEST_LIB ${item})
        endforeach()
    else()
        message(STATUS "${plugin_opt}=OFF")
    endif()
endmacro()

# helper to add plugin test
macro(add_plugin_test plugin_name test_files)
    # Convert plugin name to uppercase for the option variable
    string(TOUPPER "${plugin_name}" plugin_option)
    set(plugin_opt "PLUGIN_${plugin_option}")

    add_plugin_test_ex(${plugin_opt} "${test_files}" "../../${plugin_name}" "${NAMESPACE}${plugin_name}")
endmacro()

add_executable(${PROJECT_NAME} ${TEST_SRC})

include_directories(${TEST_INC})

target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/lib/wpeframework/plugins)

target_link_libraries(${PROJECT_NAME}
    GTest::gtest_main
    GTest::gtest
    ${TEST_LIB})

target_include_directories(${PROJECT_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks
        ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/devicesettings
        ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/thunder
        ${CMAKE_SOURCE_DIR}/../Thunder/Source/plugins
        )

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME} DISCOVERY_MODE PRE_TEST)

set (CLIENT_CORE_TEST_SRC
    tests/ClientOpenCdmCoreApiL1Tests.cpp
    tests/mocks/FakeServerInterfacesCommon.cpp
    tests/mocks/FakeServerInterfacesCoreAccessor.cpp
    ../../client/open_cdm.cpp
    ../../client/open_cdm_ext.cpp
    ../../client/open_cdm_impl.cpp
)

add_executable(OpenCDMIClientCoreL1Tests ${CLIENT_CORE_TEST_SRC})

target_include_directories(OpenCDMIClientCoreL1Tests
    PUBLIC
    ${TEST_INC}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks
    ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/devicesettings
    ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/thunder
    ${CMAKE_SOURCE_DIR}/../Thunder/Source/plugins
)

target_link_directories(OpenCDMIClientCoreL1Tests PUBLIC ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/lib/wpeframework/plugins)

target_link_libraries(OpenCDMIClientCoreL1Tests
    GTest::gtest_main
    GTest::gtest
    ${TEST_LIB})

gtest_discover_tests(OpenCDMIClientCoreL1Tests DISCOVERY_MODE PRE_TEST)

# RDK adapter tests: compile open_cdm_decrypt.cpp with GStreamer stubs so
# the pure-logic paths (null checks, key-status branching, retry flow) can
# be exercised without GStreamer or SVP hardware being present.
set(ADAPTER_RDK_TEST_SRC
    tests/AdapterRdkL1Tests.cpp
    tests/mocks/AdapterMocks.cpp
    ../../client/adapter/rdk/open_cdm_decrypt.cpp
)

add_executable(OpenCDMIAdapterRdkL1Tests ${ADAPTER_RDK_TEST_SRC})

# tests/mocks MUST come first so our gst/gst.h and gst_svp_meta.h stubs
# shadow any real GStreamer headers that may be installed on the build host.
target_include_directories(OpenCDMIAdapterRdkL1Tests
    BEFORE PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/mocks
    PUBLIC
        ../../client
        ../../client/adapter
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/thunder
        ${CMAKE_SOURCE_DIR}/../Thunder/Source/plugins
)

target_link_directories(OpenCDMIAdapterRdkL1Tests
    PUBLIC
    ${CMAKE_INSTALL_PREFIX}/lib
    ${CMAKE_INSTALL_PREFIX}/lib/wpeframework/plugins
)

target_link_libraries(OpenCDMIAdapterRdkL1Tests
    GTest::gtest_main
    GTest::gtest
)

gtest_discover_tests(OpenCDMIAdapterRdkL1Tests DISCOVERY_MODE PRE_TEST)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(TARGETS OpenCDMIClientCoreL1Tests DESTINATION bin)
install(TARGETS OpenCDMIAdapterRdkL1Tests DESTINATION bin)


