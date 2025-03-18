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

#include "roc_pyjpeg.h"

class PyRocJpegDecoder {

public:

    PyRocJpegDecoder() {};
    ~PyRocJpegDecoder() {};

    // rocJPEG API
    py::capsule rocPyJpegStreamCreate();
    py::object rocPyJpegStreamParse(py::array_t<uint8_t> file_data, size_t length, py::capsule stream_handle);
    py::capsule rocPyJpegCreate(RocJpegBackend backend, int device_id);
    std::tuple<uint8_t, RocJpegChromaSubsampling, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>>
    rocPyJpegGetImageInfo(py::capsule decode_handle, py::capsule stream_handle);
    py::object rocPyJpegDecode(PyRocJpegDecodeParams &in_decode_params, py::capsule decode_handle, py::capsule stream_handle, void *image);
    py::object rocPyJpegDecodeBatched(py::capsule decode_handle_capsule, py::list stream_capsules, int batch_size, py::list in_decode_params, py::capsule destinations_capsule);
};

class PyRocJpegUtils : public RocJpegUtils {

public:

    PyRocJpegUtils() {};
    ~PyRocJpegUtils() {};

    // rocJPEG Utils APIs
    std::tuple<std::string, std::vector<std::string>, bool, bool>
    PyGetFilePaths(std::string &input_path, std::vector<std::string> &file_paths, bool &is_dir, bool &is_file);
    std::string
    PyGetOutputFileExt(PyRocJpegDecodeParams &in_decode_params, std::string &base_file_name, uint32_t image_width, uint32_t image_height, RocJpegChromaSubsampling subsampling, std::string &image_save_path);
    py::object
    PySaveImage(PyRocJpegDecodeParams &in_decode_params, std::string image_save_path, uint32_t img_width, uint32_t img_height, RocJpegChromaSubsampling subsampling, void *image);
    std::tuple<int, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>>
    PyGetChannelPitchAndSizes(PyRocJpegDecodeParams &in_decode_params, RocJpegChromaSubsampling subsampling, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &widths, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &heights, void *image);
    std::string PyGetChromaSubsamplingStr(RocJpegChromaSubsampling subsampling);
    std::tuple<RocJpegStatus,uintptr_t> PyAllocHipDeviceMemory(uint32_t &channel_size);
    py::object PyFreeHipDeviceMemory(uintptr_t output_image_channel);
    py::object PyInitHipDevice(int device_id);
};

#endif // PY_ROC_JPEG_HEADER