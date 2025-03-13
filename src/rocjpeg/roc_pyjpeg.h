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

#ifndef PY_ROC_JPEG_PYBIND11_HEADER
#define PY_ROC_JPEG_PYBIND11_HEADER
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

#include "rocjpeg.h"
#include "rocjpeg_samples_utils.h"

#include <pybind11/pybind11.h>	 
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <iostream>
#include <pybind11/embed.h>
#include <pybind11/eval.h>

namespace py = pybind11;

// Define CropRectangle struct
struct CropRectangle {
    int16_t left;
    int16_t top;
    int16_t right;
    int16_t bottom;
};

// Define TargetDimension struct
struct TargetDimension {
    uint32_t width;
    uint32_t height;
};

// Re-Define Main Struct RocJpegDecodeParams to contain the 2 split structures
typedef struct {
    RocJpegOutputFormat output_format;
    CropRectangle crop_rectangle;
    TargetDimension target_dimension;
} PyRocJpegDecodeParams;

struct PyRocJpegImage {
    std::array<uint8_t*, ROCJPEG_MAX_COMPONENT> channel;
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> pitch;
};

// defined in roc_pyjpegdecoder.cpp
void PyRocJpegDecoderInitializer(py::module& m);
void PyRocJpegUtilsInitializer(py::module& m);

#endif // PY_ROC_JPEG_PYBIND11_HEADER