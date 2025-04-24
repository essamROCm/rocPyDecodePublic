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

#ifndef PY_ROC_JPEG_HEADER
#define PY_ROC_JPEG_HEADER
#pragma once

#include "rocjpeg/roc_pyjpeg.h"
#include "roc_pyjpeg_utils.h"
#include "roc_pyjpeg_decode_source.h"

#define MAX_SINGLE_DECODE   16  // keep records up to this count of decoded single images, higher means user need to use Batched decode instead

class Decoder {

public:

    Decoder(int device_id, int backend, RocJpegOutputFormat output_format = ROCJPEG_OUTPUT_RGB);
    ~Decoder();

    PyJpegImages decode(DecodeSource* data);
    std::vector<PyJpegImages> decode(std::vector<DecodeSource*>& data_list);

    static void exportToPython(py::module& m);

    // reset what was allocated and filled before
    void ResetCodeStreams(std::vector<CodeStream>& cs);
    void ResetImages(std::vector<PyJpegImages>& imgs);

    // set batch size for batch process
    void SetOutputFormat(RocJpegOutputFormat output_format);

    // STORE: keep code_stream(s) info here
    std::vector<CodeStream> code_stream;        // one image instance
    std::vector<CodeStream> code_stream_single; // for single decode instances (up to MAX_SINGLE_DECODE)

    // STORE: to export IMAGE/IMAGES-BATCH to python
    std::vector<PyJpegImages> images_;          // one batch instance
    std::vector<PyJpegImages> images_single;    // for single decode instances (up to MAX_SINGLE_DECODE)

    static RocJpegHandle GetHandle() {return rocjpeg_handle;};
    static void SetHandle(RocJpegHandle h) { rocjpeg_handle = h;};
    static RocJpegOutputFormat get_format() {return user_output_format;};
    static void set_format(RocJpegOutputFormat fmt) { user_output_format = fmt;};

private:
    int m_device_id;
    RocJpegBackend m_backend;
    static RocJpegHandle rocjpeg_handle;     // main session
    static RocJpegOutputFormat user_output_format;    // dynamically adjusted by the user

    bool ToDlpackTensor(CodeStream* code_stream, PyJpegImages* image);
    bool GetOutputDims(std::vector<uint32_t>& widths, std::vector<uint32_t>& heights, uint32_t img_width, uint32_t img_height, RocJpegOutputFormat output_format, RocJpegChromaSubsampling subsampling);
};

#endif // PY_ROC_JPEG_HEADER