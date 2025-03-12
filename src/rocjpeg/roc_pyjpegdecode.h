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

    PyRocJpegDecoder() {};
    ~PyRocJpegDecoder() {};

    // rocJPEG API
    py::object rocPyJpegStreamCreate();
    py::object rocPyJpegStreamParse(py::array_t<uint8_t> file_data, size_t length, RocJpegStreamHandle jpeg_stream_handle);
    py::object rocPyJpegStreamDestroy(RocJpegStreamHandle jpeg_stream_handle);
    py::object rocPyJpegCreate(RocJpegBackend backend, int device_id);
    py::object rocPyJpegDestroy(RocJpegHandle handle);
    std::tuple<uint8_t, RocJpegChromaSubsampling, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>>
    rocPyJpegGetImageInfo(RocJpegHandle handle, RocJpegStreamHandle jpeg_stream_handle);
    PyRocJpegImage rocPyJpegDecode(PyRocJpegDecodeParams &m_decode_params, RocJpegHandle rocjpeg_handle, RocJpegStreamHandle rocjpeg_stream_handle, PyRocJpegImage& py_img);
    py::object rocPyJpegDecodeBatched(RocJpegHandle handle, RocJpegStreamHandle *jpeg_stream_handles, int batch_size, const PyRocJpegDecodeParams *m_decode_params, RocJpegImage *destinations);
    std::tuple<int, int>
    rocPyInitDecodeParams(PyRocJpegDecodeParams &m_decode_params, int output_format, int left, int top, int right, int bottom);
    PyRocJpegImage rocPyAllocHipDeviceMemory(int num_channels, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &channel_sizes, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &prior_channel_sizes, PyRocJpegImage& py_img);
    PyRocJpegImage rocPyFreeHipDeviceMemory(int num_channels, PyRocJpegImage& py_img);
};

class PyRocJpegUtils : public RocJpegUtils {

public:

    PyRocJpegUtils() {};
    ~PyRocJpegUtils() {};

    // rocJPEG Utils APIs
    std::tuple<std::string, std::vector<std::string>, bool, bool>
    PyGetFilePaths(std::string &input_path, std::vector<std::string> &file_paths, bool &is_dir, bool &is_file);
    std::string
    PyGetOutputFileExt(PyRocJpegDecodeParams &m_decode_params, std::string &base_file_name, uint32_t image_width, uint32_t image_height, RocJpegChromaSubsampling subsampling, std::string &image_save_path);
    py::object
    PySaveImage(PyRocJpegDecodeParams &m_decode_params, std::string image_save_path, uint32_t img_width, uint32_t img_height, RocJpegChromaSubsampling subsampling, PyRocJpegImage& py_img);
    std::tuple<int, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>>
    PyGetChannelPitchAndSizes(PyRocJpegDecodeParams &m_decode_params, RocJpegChromaSubsampling subsampling, 
                                std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &widths, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &heights, PyRocJpegImage& py_img);
    std::string PyGetChromaSubsamplingStr(RocJpegChromaSubsampling subsampling);
};

#endif // PY_ROC_JPEG_HEADER