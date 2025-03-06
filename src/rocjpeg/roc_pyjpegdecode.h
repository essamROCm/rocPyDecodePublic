/*
Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef PY_ROC_JPEG_HEADER
#define PY_ROC_JPEG_HEADER
#pragma once

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <functional>
#include <condition_variable>
#include <queue>
#if __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
    namespace fs = std::filesystem;
#else
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#endif
#include <chrono>

#include "roc_pyjpeg.h"


class PyRocJpegDecoder : public RocJpegUtils {

public:

    PyRocJpegDecoder(){};
    ~PyRocJpegDecoder(){};

    void InitConfigStructure();

    // rocJPEG API

    py::object rocPyJpegStreamCreate();

    py::object rocPyJpegStreamParse(py::array_t<uint8_t> file_data, size_t length, RocJpegStreamHandle jpeg_stream_handle);

    py::object rocPyJpegStreamDestroy(RocJpegStreamHandle jpeg_stream_handle);

    py::object rocPyJpegCreate(RocJpegBackend backend, int device_id);

    py::object rocPyJpegDestroy(RocJpegHandle handle);

    std::tuple<uint8_t,RocJpegChromaSubsampling,uint32_t,uint32_t>
    rocPyJpegGetImageInfo(RocJpegHandle handle, RocJpegStreamHandle jpeg_stream_handle);

    py::object rocPyJpegDecode(RocJpegHandle handle, RocJpegStreamHandle jpeg_stream_handle, const RocJpegDecodeParams *decode_params, RocJpegImage *destination);
    py::object rocPyJpegDecodeBatched(RocJpegHandle handle, RocJpegStreamHandle *jpeg_stream_handles, int batch_size, const RocJpegDecodeParams *decode_params, RocJpegImage *destinations);

    // Utils API

    std::tuple<std::string, std::vector<std::string>, bool, bool>
    PyGetFilePaths(std::string &input_path, std::vector<std::string> &file_paths, bool &is_dir, bool &is_file);

    py::object PyInitHipDevice(int device_id);

    std::string PyGetChromaSubsamplingStr(RocJpegChromaSubsampling subsampling);

};

#endif // PY_ROC_JPEG_HEADER