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
extern RocJpegOutputFormat user_output_format;

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
    }

    ~PyJpegImages() {};

    void set_valid(bool state) {valid = state;};
    bool is_valid() {return valid;};

    // The image in the GPU MEM represented with dlpack via this ext_buf (for external buffer)
    std::vector<std::shared_ptr<BufferInterface>> ext_buf; // external buffer, a view on the GPU MEM of the decoded image

    py::array_t<uint8_t> to_numpy(int index = 0) {
        py::array_t<uint8_t> ret;
        if (index < 0 || index >= static_cast<int>(ext_buf.size()))
            throw std::out_of_range("Invalid channel index");
        auto& buf = ext_buf[index];
        uint8_t* data_ptr = static_cast<uint8_t*>(buf->data());
        py::tuple py_shape = buf->shape();
        if (py_shape.size() == 3) {
            const ssize_t height   = py_shape[0].cast<ssize_t>();
            const ssize_t width    = py_shape[1].cast<ssize_t>();
            const ssize_t channels = py_shape[2].cast<ssize_t>();
            std::vector<ssize_t> shape   = { height, width, channels };
            std::vector<ssize_t> strides = { width * channels, channels, 1 };
            ret = py::array_t<uint8_t>(shape, strides, data_ptr, py::cast(buf));
        } else if (py_shape.size() == 2) {
            const ssize_t height = py_shape[0].cast<ssize_t>();
            const ssize_t width  = py_shape[1].cast<ssize_t>();
            const ssize_t row_stride = buf->strides()[0].cast<ssize_t>();
            std::vector<ssize_t> shape   = { height, width };
            std::vector<ssize_t> strides = { row_stride, 1 };
            ret = py::array_t<uint8_t>(shape, strides, data_ptr, py::cast(buf));
        } else {
            throw std::runtime_error("Unsupported shape: only 2D or 3D supported");
        }
        return ret;
    }
    // flag indicates this instance of the PyJpegImages is valid to use
    // when false means the file/data associated with it to be decoded
    // found invalid, corrupted or has issues prevent from decoding it properly
protected:
    bool valid = false;
};

#endif // PY_ROC_JPEG_PYBIND11_HEADER