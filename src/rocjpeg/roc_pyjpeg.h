/*
Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
#include "rocjpeg/rocjpeg.h"
#include "roc_pyjpeg_buffer.h"

#include <pybind11/pybind11.h>	 
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/complex.h>
#include <pybind11/chrono.h>

namespace py = pybind11;
using namespace py::literals;

extern RocJpegHandle rocjpeg_handle;

#define PY_CHECK_DECODER() {        \
    if (!rocjpeg_handle) {          \
        std::cerr << "ERROR: Decoder is not instantiated. Please create/instantiate the Decoder class first." << std::endl; \
        std::exit(EXIT_FAILURE);    \
    }                               \
}

#include <pybind11/numpy.h>

class PyJpegImages {

public:
    PyJpegImages() {
        // init GPU mem -python buffer
        ext_buf.push_back(std::make_shared<BufferInterface>());
        ext_buf.push_back(std::make_shared<BufferInterface>());
        ext_buf.push_back(std::make_shared<BufferInterface>());
        // init CPU mem -python buffer
        cpu_data_temp_8bits = nullptr;
    }

    ~PyJpegImages(){
        if(cpu_data_temp_8bits) {
            hipError_t hip_status = hipFree((void *)cpu_data_temp_8bits);
            cpu_data_temp_8bits = nullptr;
        }
    };

    // to export as GPU MEM (dlpack)
    std::vector<std::shared_ptr<BufferInterface>> ext_buf;

    // to export as numpy array (HOST MEM)
    uint8_t* cpu_data_temp_8bits = nullptr;

    // export numpy 8 bits array (HOST MEM)
    py::array_t<uint8_t> to_numpy_8bits(int index = 0) {
        if (index < 0 || index >= static_cast<int>(ext_buf.size()))
            throw std::out_of_range("Invalid channel index");
        auto& buf = ext_buf[index];
        uint8_t* data_ptr = static_cast<uint8_t*>(buf->data());
        py::tuple py_shape = buf->shape();
        py::tuple py_strides = buf->strides();
        std::vector<ssize_t> shape, strides;
        for (auto item : py_shape)
            shape.push_back(item.cast<ssize_t>());
        for (auto item : py_strides)
            strides.push_back(item.cast<ssize_t>());
        return py::array_t<uint8_t>(shape, strides, cpu_data_temp_8bits, py::cast(buf));
    }
};

#endif // PY_ROC_JPEG_PYBIND11_HEADER