################################################################################
# Copyright (c) 2024 Advanced Micro Devices, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to dec
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
################################################################################

cmake_minimum_required(VERSION 3.15)
# ROCM Path
if(DEFINED ENV{ROCM_PATH})
    set(ROCM_PATH $ENV{ROCM_PATH} CACHE PATH "Default ROCm installation path")
elseif(ROCM_PATH)
    message("-- INFO:ROCM_PATH Set -- ${ROCM_PATH}")
else()
    set(ROCM_PATH /opt/rocm CACHE PATH "Default ROCm installation path")
endif()
# Set AMD Clang as default compiler
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS ON)
if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_C_COMPILER ${ROCM_PATH}/lib/llvm/bin/amdclang)
    set(CMAKE_CXX_COMPILER ${ROCM_PATH}/lib/llvm/bin/amdclang++)
endif()

# lib version - NOTE: update version across multiple files
set(VERSION "0.8.0")

# Set Project Version and Language
project(rocpydecode VERSION ${VERSION} LANGUAGES CXX)
set(TARGET_NAME rocpydecode)
set(TARGET_NAME_JPEG rocpyjpegdecode)

set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "Library install directory")
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE STRING "Include install directory")
include(GNUInstallDirs)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})

find_program(MAKE_NSIS_EXE makensis)
find_program(RPMBUILD_EXE rpmbuild)
find_program(DPKG_EXE dpkg)

# avoid setting the default installation path to /usr/local
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${ROCM_PATH} CACHE PATH "rocPyDecode default installation path" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

set(DEFAULT_BUILD_TYPE "Release")
set(ENHANCED_MESSAGE ON)

if(ENHANCED_MESSAGE)
    string(ASCII 27 Esc)
    set(ColourReset "${Esc}[m")
    set(Red "${Esc}[31m")
    set(Green "${Esc}[32m")
    set(Yellow "${Esc}[33m")
    set(Blue "${Esc}[34m")
    set(BoldBlue "${Esc}[1;34m")
    set(Magenta "${Esc}[35m")
    set(Cyan "${Esc}[36m")
    set(White "${Esc}[37m")
endif()
message("-- ${BoldBlue}rocPyDecode Version -- ${VERSION}${ColourReset}")
message("-- ${BoldBlue}rocPyDecode Install Path -- ${CMAKE_INSTALL_PREFIX}${ColourReset}")

# CMake Config location
list(APPEND CMAKE_PREFIX_PATH ${ROCM_PATH}/lib/cmake)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "rocPyDecode Default Build Type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    # -O0 -- Don't Optimize output file
    # -gdwarf-4  -- generate debugging information, dwarf-4 for making valgrind work
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -gdwarf-4")
else()
    # -O3       -- Optimize output file 
    # -DNDEBUG  -- turn off asserts 
    # -fPIC     -- Generate position-independent code if possible
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG -fPIC")
endif()

# Set supported GPU Targets
if(NOT GPU_TARGETS AND NOT AMDGPU_TARGETS)
    set(DEFAULT_GPU_TARGETS "gfx908;gfx90a;gfx942;gfx1030;gfx1031;gfx1032;gfx1100;gfx1101;gfx1102;gfx950;gfx1200;gfx1201")
endif()

# Set AMD GPU_TARGETS
if((AMDGPU_TARGETS OR DEFINED ENV{AMDGPU_TARGETS}) AND (NOT GPU_TARGETS))
    message("-- ${Red}${PROJECT_NAME} DEPRECATION -- AMDGPU_TARGETS use is deprecated. Use GPU_TARGETS${ColourReset}")
    if(DEFINED ENV{AMDGPU_TARGETS})
        set(GPU_TARGETS $ENV{AMDGPU_TARGETS} CACHE STRING "List of specific machine types for library to target")
    else()
        set(GPU_TARGETS ${AMDGPU_TARGETS})
    endif()
endif()

if(DEFINED ENV{GPU_ARCHS})
    set(GPU_TARGETS $ENV{GPU_ARCHS} CACHE STRING "List of specific machine types for library to target")
elseif(GPU_TARGETS)
    message("-- ${White}${PROJECT_NAME} -- GPU_TARGETS set with -D option${ColourReset}")
else()
    set(GPU_TARGETS "${DEFAULT_GPU_TARGETS}" CACHE STRING "List of specific machine types for library to target")
endif()
message("-- ${White}${PROJECT_NAME} -- AMD GPU_TARGETS: ${GPU_TARGETS}${ColourReset}")

# Allow specifying a Python version via -D PYTHON_VERSION_SUGGESTED
option(PYTHON_VERSION_SUGGESTED "Suggested Python version for installation" "")
# Fixed Python version list (same as in rocAL_pybind)
set(PYTHON_VERSIONS "3.13" "3.12" "3.11" "3.10" "3.9" "3.8")
if(PYTHON_VERSION_SUGGESTED AND NOT (PYTHON_VERSION_SUGGESTED STREQUAL ""))
    set(PYTHON_VERSIONS "${PYTHON_VERSION_SUGGESTED}")
    message("-- ${White}USER DEFINED PYTHON VERSION LIST: ${PYTHON_VERSIONS}${ColourReset}")
endif()
set(CMAKE_INSTALL_PREFIX_PYTHON ${CMAKE_INSTALL_PREFIX})

# rocPyDecode build
set(CMAKE_SKIP_BUILD_RPATH true)

# Find dependencies
find_package(HIP REQUIRED)
find_package(rocdecode 1.0.0 REQUIRED)
find_package(rocjpeg 1.0.0 REQUIRED)
find_package(pybind11 REQUIRED)
find_package(dlpack REQUIRED)
find_package(FFmpeg REQUIRED)

# HIP
set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} hip::device)
# rocDecode
include_directories(${rocdecode_INCLUDE_DIR}
    ${ROCM_PATH}/include/rocdecode
    ${ROCM_PATH}/share/rocdecode/utils
    ${ROCM_PATH}/share/rocdecode/utils/rocvideodecode
)
if(rocdecode_VERSION VERSION_LESS_EQUAL 1.3.0)
    find_library(rocdecode_HOST_LIBRARY rocdecodehost PATHS ${ROCM_PATH}/lib)
else()
    find_library(rocdecode_HOST_LIBRARY rocdecode-host PATHS ${ROCM_PATH}/lib)
endif()

set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} rocdecode::rocdecode)
if(rocdecode_HOST_LIBRARY)
    set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${rocdecode_HOST_LIBRARY})
    message(STATUS "rocdecode_HOST_LIBRARY found")
else()
    message(FATAL_ERROR "Library rocdecode_HOST_LIBRARY not found!")
endif()
# FFMPEG
include_directories(${AVUTIL_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR})
set(LINK_LIBRARY_LIST ${LINK_LIBRARY_LIST} ${FFMPEG_LIBRARIES})
# rocPyDecode 
include_directories(src)
file(GLOB pyfiles pyRocVideoDecode/*.py pyRocVideoDecode/*.pyi)
file(GLOB include src/rocdecode/*.h src/common/*.h ${ROCM_PATH}/share/rocdecode/utils/rocvideodecode/*.h ${ROCM_PATH}/share/rocdecode/utils/ffmpegvideodecode/*.h)
file(GLOB sources src/rocdecode/*.cpp src/common/*.cpp ${ROCM_PATH}/share/rocdecode/utils/*.cpp ${ROCM_PATH}/share/rocdecode/utils/rocvideodecode/*.cpp ${ROCM_PATH}/share/rocdecode/utils/ffmpegvideodecode/*.cpp)
# rocJPEG
include_directories(${rocjpeg_INCLUDE_DIR} ${ROCM_PATH}/share/rocjpeg/samples)
set(LINK_LIBRARY_LIST_JPEG ${LINK_LIBRARY_LIST_JPEG} hip::device rocjpeg::rocjpeg)

file(GLOB pyfiles_jpeg pyRocJpegDecode/*.py pyRocJpegDecode/*.pyi)
file(GLOB include_jpeg src/rocjpeg/*.h src/common/*.h)
file(GLOB sources_jpeg src/rocjpeg/*.cpp src/common/*.cpp)

message("-- ${White}rocPyDecode -- Link Libraries: ${LINK_LIBRARY_LIST}${ColourReset}")
message("-- ${White}rocPyJPEG -- Link Libraries: ${LINK_LIBRARY_LIST_JPEG}${ColourReset}")
message("-- ${White}rocPyDecode/rocPyJPEG -- CMAKE_CXX_FLAGS:${CMAKE_CXX_FLAGS}${ColourReset}")

# set license information
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
install(FILES ${CPACK_RESOURCE_FILE_LICENSE} DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT runtime)
install(FILES ${CPACK_RESOURCE_FILE_LICENSE} DESTINATION ${CMAKE_INSTALL_DOCDIR}-asan COMPONENT asan)

# Install rocPyDecode pybind lib (fixed version list, CMake Python discovery)
set(ROCPYDECODE_INSTALLED FALSE)
set(PYTHON_INDEX 0)

foreach(PY_VERSION ${PYTHON_VERSIONS})
    message(STATUS "Searching for Python version: ${PY_VERSION}")

    foreach(v IN ITEMS
        Python3_EXECUTABLE
        Python3_VERSION
        Python3_SOABI
        Python3_EXTENSION_SUFFIX
        Python3_Interpreter_FOUND
        Python3_Development_FOUND
        Python3_INCLUDE_DIRS
        Python3_LIBRARY
        Python3_LIBRARIES
        Python3_STDARCH
        Python3_FIND_IMPLEMENTATIONS
    )
        unset(${v} CACHE)
        unset(${v})
    endforeach()

    set(Python3_FIND_VIRTUALENV FIRST)
    find_package(Python3 "${PY_VERSION}" EXACT QUIET COMPONENTS Interpreter Development)

    if(NOT Python3_FOUND)
        message("-- ${Yellow}NOTE: Python${PY_VERSION} Not Found, not building rocPyDecode for Python${PY_VERSION}${ColourReset}")
        continue()
    endif()

    # Ensure we have a standard extension suffix (e.g., .cpython-312-x86_64-linux-gnu.so)
    if(NOT Python3_EXTENSION_SUFFIX OR Python3_EXTENSION_SUFFIX STREQUAL "")
        if(Python3_SOABI)
            set(Python3_EXTENSION_SUFFIX ".${Python3_SOABI}.so")
        else()
            set(Python3_EXTENSION_SUFFIX ".so")
        endif()
    endif()

    math(EXPR PYTHON_INDEX "${PYTHON_INDEX} + 1")
    set(TARGET_NAME_VERSIONED "${TARGET_NAME}_${PY_VERSION}")
    set(TARGET_NAME_JPEG_VERSIONED "${TARGET_NAME_JPEG}_${PY_VERSION}")

    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(CMAKE_INSTALL_PREFIX_PYTHON ${Python3_STDARCH} CACHE PATH "rocPyDecode Python install path" FORCE)
    else()
        set(CMAKE_INSTALL_PREFIX_PYTHON ${CMAKE_INSTALL_PREFIX} CACHE PATH "rocPyDecode Python install path" FORCE)
    endif()
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

    # Build rocPyDecode module for this Python version
    pybind11_add_module(${TARGET_NAME_VERSIONED} MODULE ${sources})
    target_link_libraries(${TARGET_NAME_VERSIONED} PRIVATE ${LINK_LIBRARY_LIST} ${Python3_LIBRARIES})
    target_include_directories(${TARGET_NAME_VERSIONED} PRIVATE ${pybind11_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS})

    if(_FFMPEG_AVCODEC_VERSION VERSION_LESS_EQUAL 58.134.100)
        target_compile_definitions(${TARGET_NAME_VERSIONED} PUBLIC USE_AVCODEC_GREATER_THAN_58_134=0)
    else()
        target_compile_definitions(${TARGET_NAME_VERSIONED} PUBLIC USE_AVCODEC_GREATER_THAN_58_134=1)
    endif()

    foreach(filename ${pyfiles})
        get_filename_component(target "${filename}" REALPATH)
        file(RELATIVE_PATH ITEM_PATH_REL ${CMAKE_CURRENT_SOURCE_DIR} ${filename})
        message(STATUS "Copying ${filename} to ${TARGET_NAME_VERSIONED}/${ITEM_PATH_REL}")
        configure_file("${filename}" "${CMAKE_BINARY_DIR}/${TARGET_NAME_VERSIONED}/${ITEM_PATH_REL}" COPYONLY)
    endforeach(filename)

    set_target_properties(${TARGET_NAME_VERSIONED} PROPERTIES
        PREFIX ""
        SUFFIX "${Python3_EXTENSION_SUFFIX}"
        OUTPUT_NAME "${TARGET_NAME}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${TARGET_NAME}/${CMAKE_INSTALL_LIBDIR}")

    set(ROCPYDECODE_INSTALLED TRUE)
    message(STATUS "Building and installing rocPyDecode for Python ${PY_VERSION}")
    install(TARGETS ${TARGET_NAME_VERSIONED} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime)

    # Build rocPyJpegDecode module for this Python version
    pybind11_add_module(${TARGET_NAME_JPEG_VERSIONED} MODULE ${sources_jpeg})
    target_link_libraries(${TARGET_NAME_JPEG_VERSIONED} PRIVATE ${LINK_LIBRARY_LIST_JPEG} ${Python3_LIBRARIES})
    target_include_directories(${TARGET_NAME_JPEG_VERSIONED} PRIVATE ${pybind11_INCLUDE_DIRS} ${Python3_INCLUDE_DIRS})

    foreach(filename ${pyfiles_jpeg})
        get_filename_component(target "${filename}" REALPATH)
        file(RELATIVE_PATH ITEM_PATH_REL ${CMAKE_CURRENT_SOURCE_DIR} ${filename})
        message(STATUS "Copying ${filename} to ${TARGET_NAME_JPEG_VERSIONED}/${ITEM_PATH_REL}")
        configure_file("${filename}" "${CMAKE_BINARY_DIR}/${TARGET_NAME_JPEG_VERSIONED}/${ITEM_PATH_REL}" COPYONLY)
    endforeach(filename)

    set_target_properties(${TARGET_NAME_JPEG_VERSIONED} PROPERTIES
        PREFIX ""
        SUFFIX "${Python3_EXTENSION_SUFFIX}"
        OUTPUT_NAME "${TARGET_NAME_JPEG}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${TARGET_NAME_JPEG}/${CMAKE_INSTALL_LIBDIR}")

    message(STATUS "Building and installing rocpyjpegdecode for Python ${PY_VERSION}")
    install(TARGETS ${TARGET_NAME_JPEG_VERSIONED} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime)
endforeach()

# Was rocPyDecode Installed?
if(NOT ROCPYDECODE_INSTALLED)
    message("-- ${Red}rocPyDecode wasn't installed, no python installation was found.${ColourReset}")
endif()

#install rocPyDecode API folder -and- samples folder
install(DIRECTORY pyRocVideoDecode DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime)
install(DIRECTORY samples/rocdecode DESTINATION ${CMAKE_INSTALL_LIBDIR}/pyRocVideoDecode/samples COMPONENT runtime)
#install rocpyjpegdecode API folder -and- samples folder
install(DIRECTORY pyRocJpegDecode DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime)
install(DIRECTORY samples/rocjpeg DESTINATION ${CMAKE_INSTALL_LIBDIR}/pyRocJpegDecode/samples COMPONENT runtime)

# Test - Installed -rocpydecode
install(DIRECTORY ${CMAKE_SOURCE_DIR}/tests DESTINATION ${CMAKE_INSTALL_DATADIR}/rocpydecode COMPONENT test)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/cmake DESTINATION ${CMAKE_INSTALL_DATADIR}/rocpydecode COMPONENT test)

message("-- ${Green}ROCm Video Decode Library Python Binding - rocPyDecode module added ${ColourReset}")
message("-- ${BoldBlue}rocPyDecode Version -- ${VERSION}${ColourReset}")
message("-- ${BoldBlue}rocPyDecode Install Path -- ${CMAKE_INSTALL_PREFIX_PYTHON}${ColourReset}")

# add symlinks to videos exist in rocDecode shared folder
set(CP_SYMLINK "ln -s ${ROCM_PATH}/share/rocdecode/video/AMD_driving_virtual_20-* data/videos/.")
execute_process(COMMAND /bin/sh -c ${CP_SYMLINK} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} RESULT_VARIABLE LN_RESULT ERROR_VARIABLE LN_ERROR)
message("-- ${BoldBlue}Created symlink(s) to video files exist under rocdecode shared util folder.${ColourReset}")

# make test with CTest
enable_testing()
include(CTest)
add_subdirectory(tests)

# set package information
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
set(CPACK_PACKAGE_LICENSE "MIT")
# TBD: Change to generic support email
set(CPACK_PACKAGE_CONTACT "rocPyDecode Support <mivisionx.support@amd.com>")
set(CPACK_PACKAGE_VENDOR "AMD ROCm")
set(CPACK_PACKAGE_GROUP "Development/Tools")
set(CPACK_PACKAGE_HOMEPAGE "https://github.com/ROCm/rocPyDecode")

if(ENABLE_ASAN_PACKAGING)
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "AMD rocPyDecode address sanitizer libraries")
    set(CPACK_RPM_PACKAGE_SUMMARY "AMD rocPyDecode address sanitizer libraries")
else()
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "AMD rocPyDecode is a python high performance video decode SDK for AMD GPUs")
    set(CPACK_RPM_PACKAGE_SUMMARY "AMD rocPyDecode is a python high performance video decode SDK for AMD GPUs")
endif()

if(DEFINED ENV{ROCM_LIBPATCH_VERSION})
    set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}.$ENV{ROCM_LIBPATCH_VERSION}")
endif()

# Set the dependent packages
set(ROCPYDECODE_DEBIAN_PACKAGE_LIST "rocdecode-dev, rocjpeg-dev, libdlpack-dev, python3-dev, python3-pip")
# TBD: libdlpack-devel not available
set(ROCPYDECODE_RPM_PACKAGE_LIST "rocdecode-devel, rocjpeg-devel, python3-devel, python3-pip")

# '%{?dist}' breaks manual builds on debian systems due to empty Provides
execute_process(
    COMMAND rpm --eval %{?dist}
    RESULT_VARIABLE PROC_RESULT
    OUTPUT_VARIABLE EVAL_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(PROC_RESULT EQUAL "0" AND NOT EVAL_RESULT STREQUAL "")
    string(APPEND CPACK_RPM_PACKAGE_RELEASE "%{?dist}")
endif()

# set package 
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${CPACK_PACKAGE_HOMEPAGE})
set(CPACK_RPM_PACKAGE_URL ${CPACK_PACKAGE_HOMEPAGE})
set(CPACK_RPM_PACKAGE_AUTOREQPROV "no")
# Debian package
set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS "rocm-core, ${ROCPYDECODE_DEBIAN_PACKAGE_LIST}")
set(CPACK_DEBIAN_RUNTIME_PACKAGE_LICENSE "MIT")
# Debian package - specific variable for ASAN
set(CPACK_DEBIAN_ASAN_PACKAGE_NAME "${PROJECT_NAME}-asan")
set(CPACK_DEBIAN_ASAN_PACKAGE_DEPENDS "rocm-core-asan, ${ROCPYDECODE_DEBIAN_PACKAGE_LIST}")
# Debian package - Test
set(CPACK_DEBIAN_TEST_PACKAGE_NAME "${PROJECT_NAME}-test")
set(CPACK_DEBIAN_TEST_PACKAGE_DEPENDS "rocm-core, ${CPACK_DEBIAN_RUNTIME_PACKAGE_NAME}")
# RPM package
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_RUNTIME_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_RPM_RUNTIME_PACKAGE_REQUIRES "rocm-core, ${ROCPYDECODE_RPM_PACKAGE_LIST}")
set(CPACK_RPM_RUNTIME_PACKAGE_LICENSE "MIT")
# RPM package specific variable for ASAN
set(CPACK_RPM_ASAN_PACKAGE_NAME "${PROJECT_NAME}-asan")
set(CPACK_RPM_ASAN_PACKAGE_REQUIRES "rocm-core-asan, ${ROCPYDECODE_RPM_PACKAGE_LIST}")
# RPM package specific variable for Test
set(CPACK_RPM_TEST_PACKAGE_NAME "${PROJECT_NAME}-test")
set(CPACK_RPM_TEST_PACKAGE_REQUIRES "rocm-core, ${CPACK_RPM_RUNTIME_PACKAGE_NAME}")

if(NOT ROCM_DEP_ROCMCORE)
    string(REGEX REPLACE ",? ?rocm-core," "" CPACK_RPM_RUNTIME_PACKAGE_REQUIRES ${CPACK_RPM_RUNTIME_PACKAGE_REQUIRES})
    string(REGEX REPLACE ",? ?rocm-core-asan," "" CPACK_RPM_ASAN_PACKAGE_REQUIRES ${CPACK_RPM_ASAN_PACKAGE_REQUIRES})
    string(REGEX REPLACE ",? ?rocm-core," "" CPACK_RPM_TEST_PACKAGE_REQUIRES ${CPACK_RPM_TEST_PACKAGE_REQUIRES})
    string(REGEX REPLACE ",? ?rocm-core," "" CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS ${CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS})
    string(REGEX REPLACE ",? ?rocm-core-asan," "" CPACK_DEBIAN_ASAN_PACKAGE_DEPENDS ${CPACK_DEBIAN_ASAN_PACKAGE_DEPENDS})
    string(REGEX REPLACE ",? ?rocm-core," "" CPACK_DEBIAN_TEST_PACKAGE_DEPENDS ${CPACK_DEBIAN_TEST_PACKAGE_DEPENDS})
endif()

if(ENABLE_ASAN_PACKAGING)
    # ASAN Package requires asan component with only libraries and license file
    set(CPACK_COMPONENTS_ALL asan)
else()
    set(CPACK_COMPONENTS_ALL runtime test)
endif()

set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_RELEASE "local")
set(CPACK_RPM_PACKAGE_RELEASE "local")

if(DEFINED ENV{CPACK_DEBIAN_PACKAGE_RELEASE})
    set(CPACK_DEBIAN_PACKAGE_RELEASE $ENV{CPACK_DEBIAN_PACKAGE_RELEASE})
endif()
if(DEFINED ENV{CPACK_RPM_PACKAGE_RELEASE})
    set(CPACK_RPM_PACKAGE_RELEASE $ENV{CPACK_RPM_PACKAGE_RELEASE})
endif()

set(CPACK_GENERATOR "TGZ;ZIP")
if(EXISTS ${MAKE_NSIS_EXE})
    list(APPEND CPACK_GENERATOR "NSIS")
endif()
if(EXISTS ${RPMBUILD_EXE})
    list(APPEND CPACK_GENERATOR "RPM")
    message("-- ${White}rocPyDecode .rpm Package Requirements -- ${CPACK_RPM_RUNTIME_PACKAGE_REQUIRES}${ColourReset}")
    message("-- ${White}rocPyDecode .rpm Test Package Requirements -- ${CPACK_RPM_TEST_PACKAGE_REQUIRES}${ColourReset}")
endif()
if(EXISTS ${DPKG_EXE})
    list(APPEND CPACK_GENERATOR "DEB")
    message("-- ${White}rocPyDecode .deb Package Requirements -- ${CPACK_DEBIAN_RUNTIME_PACKAGE_DEPENDS}${ColourReset}")
    message("-- ${White}rocPyDecode .deb Test Package Requirements -- ${CPACK_DEBIAN_TEST_PACKAGE_DEPENDS}${ColourReset}")
endif()

include(CPack)

cpack_add_component(runtime
    DISPLAY_NAME "rocPyDecode Package"
    DESCRIPTION "AMD rocPyDecode is a python high performance video decode SDK for AMD GPUs")

cpack_add_component(asan
    DISPLAY_NAME "rocPyDecode ASAN Package"
    DESCRIPTION "AMD rocPyDecode is a python high performance video decode SDK for AMD GPUs")

cpack_add_component(test
    DISPLAY_NAME "rocPyDecode Test Package"
    DESCRIPTION "AMD rocPyDecode is a python high performance video decode SDK for AMD GPUs")
