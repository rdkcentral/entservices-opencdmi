# - Try to find gstreamer_base.
# Once done, this will define
#
#  GSTREAMER_BASE_FOUND - the gstreamer_base is available
#  GSTREAMER_BASE::GSTREAMER_BASE - The gstreamer_base library and all its dependecies
#
# If not stated otherwise in this file or this component's Licenses.txt file the
# following copyright and licenses apply:
#
# Copyright 2016 RDK Management
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

if(GSTREAMER_BASE_FIND_QUIETLY)
    set(_GSTREAMER_BASE_MODE QUIET)
elseif(GSTREAMER_BASE_FIND_REQUIRED)
    set(_GSTREAMER_BASE_MODE REQUIRED)
endif()

find_package(PkgConfig)
pkg_check_modules(PC_GSTREAMER_BASE ${_GSTREAMER_BASE_MODE} gstreamer-base-1.0)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GSTREAMER_BASE DEFAULT_MSG PC_GSTREAMER_BASE_FOUND PC_GSTREAMER_BASE_INCLUDE_DIRS PC_GSTREAMER_BASE_LIBRARIES)
mark_as_advanced(PC_GSTREAMER_BASE_INCLUDE_DIRS PC_GSTREAMER_BASE_LIBRARIES PC_GSTREAMER_BASE_LIBRARY_DIRS)

foreach(libraryName ${PC_GSTREAMER_BASE_LIBRARIES})
find_library(${libraryName}_GSTREAMER_BASE_SINGLE_LIB ${libraryName} HINTS ${PC_GSTREAMER_BASE_LIBRARY_DIRS})
list(APPEND ALL_GSTREAMER_BASE_LIBS "${${libraryName}_GSTREAMER_BASE_SINGLE_LIB}")
endforeach(libraryName)

if(GSTREAMER_BASE_FOUND)
    set(GSTREAMER_BASE_LIBRARIES ${ALL_GSTREAMER_BASE_LIBS})
    set(GSTREAMER_BASE_INCLUDES ${PC_GSTREAMER_BASE_INCLUDE_DIRS})

    if(NOT TARGET GSTREAMER_BASE::GSTREAMER_BASE)
        add_library(GSTREAMER_BASE::GSTREAMER_BASE UNKNOWN IMPORTED)

        set_target_properties(GSTREAMER_BASE::GSTREAMER_BASE
                PROPERTIES
                IMPORTED_LINK_INTERFACE_LANGUAGES "C"
                IMPORTED_LOCATION "${PC_GSTREAMER_BASE_LIBRARY_DIRS}"
                INTERFACE_COMPILE_DEFINITIONS "GSTREAMER"
                INTERFACE_COMPILE_OPTIONS "${PC_GSTREAMER_BASE_CFLAGS_OTHER}"
                INTERFACE_INCLUDE_DIRECTORIES "${PC_GSTREAMER_BASE_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${ALL_GSTREAMER_BASE_LIBS}"
                )
    endif()
endif()
