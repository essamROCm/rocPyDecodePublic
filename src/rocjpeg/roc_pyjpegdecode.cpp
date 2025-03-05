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

#include "roc_pyjpegdecode.h"

using namespace std;

void PyRocJpegDecoderInitializer(py::module& m) {
    py::class_<PyRocJpegDecoder> (m, "PyRocJpegDecoder")
        .def(py::init<>())
        .def("PyGetFilePaths",&PyRocJpegDecoder::PyGetFilePaths)
        .def("PyInitHipDevice",&PyRocJpegDecoder::PyInitHipDevice)
        .def("rocPyJpegCreate",&PyRocJpegDecoder::rocPyJpegCreate)
        .def("rocPyJpegStreamCreate",&PyRocJpegDecoder::rocPyJpegStreamCreate)
        .def("rocPyJpegDestroy",&PyRocJpegDecoder::rocPyJpegDestroy)
        .def("rocPyJpegStreamDestroy",&PyRocJpegDecoder::rocPyJpegStreamDestroy)
        .def("rocPyJpegStreamParse",&PyRocJpegDecoder::rocPyJpegStreamParse)
        .def("rocPyJpegGetImageInfo",&PyRocJpegDecoder::rocPyJpegGetImageInfo)
        ;
}
  

void PyRocJpegDecoder::InitConfigStructure() {
}

py::object PyRocJpegDecoder::rocPyJpegStreamCreate() {
    RocJpegStreamHandle rocjpeg_stream_handle = nullptr;
    CHECK_ROCJPEG(rocJpegStreamCreate(&rocjpeg_stream_handle));
    return py::cast(rocjpeg_stream_handle);
}

py::object PyRocJpegDecoder::rocPyJpegStreamParse(py::array_t<uint8_t> file_data, size_t length, RocJpegStreamHandle jpeg_stream_handle) {
    auto buf = file_data.request();  // Get buffer info
    uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);  // Raw pointer
    RocJpegStatus status = rocJpegStreamParse(ptr, length, jpeg_stream_handle);
    return py::cast(status);
}

py::object PyRocJpegDecoder::rocPyJpegStreamDestroy(RocJpegStreamHandle jpeg_stream_handle) {
    CHECK_ROCJPEG(rocJpegStreamDestroy(jpeg_stream_handle));
    return py::cast<py::none>(Py_None);
}

py::object PyRocJpegDecoder::rocPyJpegCreate(RocJpegBackend backend, int device_id) {
    RocJpegHandle rocjpeg_handle = nullptr;
    CHECK_ROCJPEG(rocJpegCreate(backend, device_id, &rocjpeg_handle));
    return py::cast(rocjpeg_handle);
}

py::object PyRocJpegDecoder::rocPyJpegDestroy(RocJpegHandle rocjpeg_handle) {
    CHECK_ROCJPEG(rocJpegDestroy(rocjpeg_handle));
    return py::cast<py::none>(Py_None);
}

std::tuple<uint8_t,RocJpegChromaSubsampling,uint32_t,uint32_t>
PyRocJpegDecoder::rocPyJpegGetImageInfo(RocJpegHandle rocjpeg_handle, RocJpegStreamHandle rocjpeg_stream_handle) {
    uint8_t num_components=0;
    RocJpegChromaSubsampling subsampling;
    uint32_t widths=0;
    uint32_t heights=0;
    CHECK_ROCJPEG(rocJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handle, &num_components, &subsampling, &widths, &heights));
    return std::make_tuple(num_components, subsampling, widths, heights);
}

py::object PyRocJpegDecoder::rocPyJpegDecode(RocJpegHandle handle, RocJpegStreamHandle jpeg_stream_handle, const RocJpegDecodeParams *decode_params, RocJpegImage *destination) {
    return py::cast(rocJpegDecode(handle, jpeg_stream_handle, decode_params, destination));
}

py::object PyRocJpegDecoder::rocPyJpegDecodeBatched(RocJpegHandle handle, RocJpegStreamHandle *jpeg_stream_handles, int batch_size, const RocJpegDecodeParams *decode_params, RocJpegImage *destinations) {
    return py::cast(rocJpegDecodeBatched(handle, jpeg_stream_handles, batch_size, decode_params, destinations));
}

std::tuple<std::string, std::vector<std::string>, bool, bool>
PyRocJpegDecoder::PyGetFilePaths(std::string &input_path, std::vector<std::string> &file_paths, bool &is_dir, bool &is_file) {
    is_dir = is_file = false;
    if (!RocJpegUtils::GetFilePaths(input_path, file_paths, is_dir, is_file)) {
        std::cerr << "ERROR: Failed to get input file paths!" << std::endl;
    }
    return std::make_tuple(input_path, file_paths, is_dir, is_file);
}

py::object PyRocJpegDecoder::PyInitHipDevice(int device_id) {
    bool ret = true;
    if (!(ret=RocJpegUtils::InitHipDevice(device_id))) {
        std::cerr << "ERROR: Failed to initialize HIP!" << std::endl;
    }
    return py::cast(ret);
}
