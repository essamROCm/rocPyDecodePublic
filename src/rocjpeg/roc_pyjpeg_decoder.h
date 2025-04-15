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
    std::vector<std::shared_ptr<PyJpegImages>> decode(std::vector<DecodeSource*>& data_list);

    static void exportToPython(py::module& m);

    // reset what was allocated and filled before
    void reset_code_stream_store();
    void reset_image_store();

    // set batch size for batch process
    void set_batch_size(int batch_size);

    // STORE: keep code_stream(s) info here
    std::vector<CodeStream> code_stream; // add all live instance

    // STORE: to export IMAGE/IMAGES-BATCH to python
    std::vector<PyJpegImages> image;                        // all live instance
    std::vector<std::shared_ptr<PyJpegImages>> image_list;  // only this instance batch images addresses

private:    
    int m_device_id;
    RocJpegBackend m_backend;
    bool AsGPUTensor_dlpack_8bits(CodeStream* code_stream, PyJpegImages* image);
    bool AsNumpyHostTensor_8bits(CodeStream* code_stream, PyJpegImages* image);
    bool get_widths_heights_from_output_format(std::vector<uint32_t>& widths, std::vector<uint32_t>& heights, uint32_t img_width, uint32_t img_height, RocJpegOutputFormat output_format, RocJpegChromaSubsampling subsampling);
    int m_batch_size = 2; // default, not used for now
};

#endif // PY_ROC_JPEG_HEADER