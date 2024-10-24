################################################################################
# 
# MIT License
# 
# Copyright (c) 2024 Advanced Micro Devices, Inc.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 
################################################################################################################################################################
# - Try to find rocDecode libraries and headers
# Once done this will define
#
# rocDecode_FOUND - system has rocDecode
# rocDecode_INCLUDE_DIRS - the rocDecode include directory
# rocDecode_LIBRARIES - Link these to use rocDecode
################################################################################

find_path(rocDecode_INCLUDE_DIRS
    NAMES rocdecode.h rocparser.h
    HINTS
    $ENV{rocDecode_PATH}/include/rocdecode
    PATHS
    ${rocDecode_PATH}/include/rocdecode
    /usr/local/include/
    ${ROCM_PATH}/include/rocdecode
)
mark_as_advanced(rocDecode_INCLUDE_DIRS)

find_library(rocDecode_LIBRARIES
    NAMES rocdecode
    HINTS
    $ENV{rocDecode_PATH}/lib
    $ENV{rocDecode_PATH}/lib64
    PATHS
    ${rocDecode_PATH}/lib
    ${rocDecode_PATH}/lib64
    /usr/local/lib
    ${ROCM_PATH}/lib
)
mark_as_advanced(rocDecode_LIBRARIES)

find_path(rocDecode_LIBRARIES_DIRS
    NAMES rocdecode
    HINTS
    $ENV{rocDecode_PATH}/lib
    $ENV{rocDecode_PATH}/lib64
    PATHS
    ${rocDecode_PATH}/lib
    ${rocDecode_PATH}/lib64
    /usr/local/lib
    ${ROCM_PATH}/lib
)
mark_as_advanced(rocDecode_LIBRARIES_DIRS)

if(rocDecode_LIBRARIES AND rocDecode_LIBRARIES_DIRS)
    set(rocDecode_FOUND TRUE)
endif( )

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( rocDecode
    FOUND_VAR rocDecode_FOUND
    REQUIRED_VARS
        rocDecode_INCLUDE_DIRS
        rocDecode_LIBRARIES
        rocDecode_LIBRARIES_DIRS
)

set(rocDecode_FOUND ${rocDecode_FOUND} CACHE INTERNAL "")
set(rocDecode_LIBRARIES ${rocDecode_LIBRARIES} CACHE INTERNAL "")
set(rocDecode_INCLUDE_DIRS ${rocDecode_INCLUDE_DIRS} CACHE INTERNAL "")
set(rocDecode_LIBRARIES_DIRS ${rocDecode_LIBRARIES_DIRS} CACHE INTERNAL "")

if(rocDecode_FOUND)
    # Find rocDecode Version
    if (EXISTS "${rocDecode_INCLUDE_DIRS}/rocdecode_version.h")
        file(READ "${rocDecode_INCLUDE_DIRS}/rocdecode_version.h" ROCDECODE_VERSION_FILE)
        string(REGEX MATCH "ROCDECODE_MAJOR_VERSION ([0-9]*)" _ ${ROCDECODE_VERSION_FILE})
        set(ROCDECODE_MAJOR_VERSION ${CMAKE_MATCH_1} CACHE INTERNAL "")
        string(REGEX MATCH "ROCDECODE_MINOR_VERSION ([0-9]*)" _ ${ROCDECODE_VERSION_FILE})
        set(ROCDECODE_MINOR_VERSION ${CMAKE_MATCH_1} CACHE INTERNAL "")
        string(REGEX MATCH "ROCDECODE_MICRO_VERSION ([0-9]*)" _ ${ROCDECODE_VERSION_FILE})
        set(ROCDECODE_MICRO_VERSION ${CMAKE_MATCH_1} CACHE INTERNAL "")
        message("-- ${White}Using rocDecode -- \n\tLibraries:${rocDecode_LIBRARIES} \n\tIncludes:${rocDecode_INCLUDE_DIRS}
            \n\tVersion:${ROCDECODE_MAJOR_VERSION}.${ROCDECODE_MINOR_VERSION}.${ROCDECODE_MICRO_VERSION}${ColourReset}")
    else()
        set(ROCDECODE_MAJOR_VERSION 0)
        set(ROCDECODE_MINOR_VERSION 0)
        set(ROCDECODE_MICRO_VERSION 0)
    endif()    
else()
    if(rocDecode_FIND_REQUIRED)
        message(FATAL_ERROR "{Red}FindrocDecode -- NOT FOUND${ColourReset}")
    endif()
    message( "-- ${Yellow}NOTE: FindrocDecode failed to find -- rocDecode${ColourReset}" )
endif()