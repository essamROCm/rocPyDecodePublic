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

#ifndef PY_ROC_JPEG_CODE_STREAM_HEADER
#define PY_ROC_JPEG_CODE_STREAM_HEADER

#pragma once

#include <filesystem>
#include "roc_pyjpeg.h"
#include "roc_pyjpeg_buffer.h"

// instantiat ONE class object for decoding ONE image file

class CodeStream {

public:    
    CodeStream(const std::filesystem::path&);
    CodeStream(const unsigned char*, size_t);
    CodeStream(py::bytes);
    CodeStream(py::array_t<uint8_t>);
    ~CodeStream();
    CodeStream();

    CodeStream& operator=(const CodeStream& other);

    static void exportToPython(py::module& m);
    CodeStream* handle();

    int width();
    int height();
    
    std::shared_ptr<CodeStream> code_stream_;

    // related descriptor of 'this' image
    RocJpegStreamHandle stream_handle = nullptr;
    RocJpegDecodeParams decode_params;
    RocJpegImage output_image;
    RocJpegChromaSubsampling subsampling;
    uint32_t widths[ROCJPEG_MAX_COMPONENT] = {};
    uint32_t heights[ROCJPEG_MAX_COMPONENT] = {};    
    uint32_t channel_sizes[ROCJPEG_MAX_COMPONENT] = {};
    uint32_t num_channels = 0;
    void set_valid(bool state) {valid = state;};
    bool is_valid() {return valid;};

    // flag indicates this instance of the code_stream is valid to use
    // when false means the file/data associated with it to be decoded
    // found invalid, corrupted or has issues prevent from decoding it properly
protected:
    bool valid;

private:
    // to keep a reference to the argument data, so that they are kept alive throughout the lifetime of the object
    py::bytes data_ref_bytes_;
    py::array_t<uint8_t> data_ref_arr_;

    int m_width;
    int m_height;

    int release();
    int ReadFromFile(const std::filesystem::path& filename, std::vector<char>& file_data, int& file_size); 
    int InitializeSingleImage(const std::filesystem::path& filename, const unsigned char* data, int data_size);
};

#endif // PY_ROC_JPEG_CODE_STREAM_HEADER