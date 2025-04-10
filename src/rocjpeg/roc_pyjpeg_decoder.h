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

class Decoder {

public:

    Decoder(int device_id, int backend);
    ~Decoder();

    PyJpegImages decode(DecodeSource* data);
    std::vector<PyJpegImages> decode(std::vector<DecodeSource*>& data_list);

    static void exportToPython(py::module& m);

    // reset what was allocated and filled before
    void reset_code_stream_store();
    void reset_code_streams_store();
    void reset_image_store();
    void reset_images_store();


    // STORE: keep code_stream(s) info here
    CodeStream code_stream; // single -one- process 
    std::vector<CodeStream> code_streams; // array -list-,-many-,-batch- process

    // STORE: to export IMAGE/IMAGES-BATCH to python
    PyJpegImages image;
    std::vector<PyJpegImages> images;

private:    
    int m_device_id;
    RocJpegBackend m_backend;
    void AsNumpyHostTensor_8bits(CodeStream* code_stream, PyJpegImages* image);

};

#endif // PY_ROC_JPEG_HEADER