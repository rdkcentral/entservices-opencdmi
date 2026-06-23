# - Try to find gstreamer.
# Once done, this will define
#
#  GSTREAMER_FOUND - the gstreamer is available
#  GSTREAMER::GSTREAMER - The gstreamer library and all its dependecies
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

if(GSTREAMER_FIND_QUIETLY)
    set(_GSTREAMER_MODE QUIET)
elseif(GSTREAMER_FIND_REQUIRED)
    set(_GSTREAMER_MODE REQUIRED)
endif()

find_package(PkgConfig)
pkg_check_modules(PC_GSTREAMER ${_GSTREAMER_MODE} gstreamer-1.0)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GSTREAMER DEFAULT_MSG PC_GSTREAMER_FOUND PC_GSTREAMER_INCLUDE_DIRS PC_GSTREAMER_LIBRARIES)

mark_as_advanced(PC_GSTREAMER_INCLUDE_DIRS PC_GSTREAMER_LIBRARIES PC_GSTREAMER_LIBRARY_DIRS)

set(ALL_GSTREAMER_LIBS "")
foreach(libraryName ${PC_GSTREAMER_LIBRARIES})
find_library(${libraryName}_GSTREAMER_SINGLE_LIB ${libraryName} HINTS ${PC_GSTREAMER_LIBRARY_DIRS})
list(APPEND ALL_GSTREAMER_LIBS "${${libraryName}_GSTREAMER_SINGLE_LIB}")
endforeach(libraryName)

if(GSTREAMER_FOUND)
    set(GSTREAMER_LIBRARIES ${ALL_GSTREAMER_LIBS})
    set(GSTREAMER_INCLUDES ${PC_GSTREAMER_INCLUDE_DIRS})

    if(NOT TARGET GSTREAMER::GSTREAMER)
        add_library(GSTREAMER::GSTREAMER INTERFACE IMPORTED)

        set_target_properties(GSTREAMER::GSTREAMER
                PROPERTIES
                INTERFACE_COMPILE_DEFINITIONS "GSTREAMER"
                INTERFACE_COMPILE_OPTIONS "${PC_GSTREAMER_CFLAGS_OTHER}"
                INTERFACE_INCLUDE_DIRECTORIES "${PC_GSTREAMER_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${ALL_GSTREAMER_LIBS}"
                )
    endif()
endif()
