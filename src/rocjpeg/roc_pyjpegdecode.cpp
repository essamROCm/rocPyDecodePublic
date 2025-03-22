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
        // rocPyJPEG APIs
        .def("rocPyJpegCreate",&PyRocJpegDecoder::rocPyJpegCreate)
        .def("rocPyJpegStreamCreate",&PyRocJpegDecoder::rocPyJpegStreamCreate)
        .def("rocPyJpegStreamParse",&PyRocJpegDecoder::rocPyJpegStreamParse)
        .def("rocPyJpegGetImageInfo",&PyRocJpegDecoder::rocPyJpegGetImageInfo)
        .def("rocPyJpegDecode",&PyRocJpegDecoder::rocPyJpegDecode)
        .def("rocPyJpegDecodeBatched",&PyRocJpegDecoder::rocPyJpegDecodeBatched);
}

void PyRocJpegUtilsInitializer(py::module& m) {
    py::class_<PyRocJpegUtils> (m, "PyRocJpegUtils")
        .def(py::init<>())
        // Utils
        .def("PyGetOutputFileExt",&PyRocJpegUtils::PyGetOutputFileExt)
        .def("PySaveImage",&PyRocJpegUtils::PySaveImage)
        .def("PyGetFilePaths",&PyRocJpegUtils::PyGetFilePaths)
        .def("PyGetChannelPitchAndSizes",&PyRocJpegUtils::PyGetChannelPitchAndSizes)
        .def("PyGetChromaSubsamplingStr",&PyRocJpegUtils::PyGetChromaSubsamplingStr)
        .def("PyInitHipDevice",&PyRocJpegUtils::PyInitHipDevice)
        .def("PyAllocHipDeviceMemory",&PyRocJpegUtils::PyAllocHipDeviceMemory)
        .def("PyFreeHipDeviceMemory",&PyRocJpegUtils::PyFreeHipDeviceMemory);
}

//
// rocPyJPEG APIs
//

py::tuple PyRocJpegDecoder::rocPyJpegStreamCreate() {
    RocJpegStreamHandle rocjpeg_stream_handle = nullptr;
    RocJpegStatus status = rocJpegStreamCreate(&rocjpeg_stream_handle);
    CHECK_ROCJPEG(status);
    py::capsule capsule(
        rocjpeg_stream_handle,
        "RocJpegStreamHandle",
        [](PyObject *capsule) {
            auto ptr = static_cast<RocJpegStreamHandle>(PyCapsule_GetPointer(capsule, "RocJpegStreamHandle"));
            rocJpegStreamDestroy(ptr);
        }
    );
    return py::make_tuple(capsule, status);
}

py::object PyRocJpegDecoder::rocPyJpegStreamParse(py::array_t<uint8_t> file_data, size_t length, py::capsule stream_handle) {
    auto buf = file_data.request();
    uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);
    RocJpegStreamHandle jpeg_stream_handle = stream_handle.get_pointer();
    RocJpegStatus status = rocJpegStreamParse(ptr, length, jpeg_stream_handle);
    CHECK_ROCJPEG(status);
    return py::cast(status);
}

py::tuple PyRocJpegDecoder::rocPyJpegCreate(RocJpegBackend backend, int device_id) {
    RocJpegHandle decode_handle = nullptr;
    RocJpegStatus status = rocJpegCreate(backend, device_id, &decode_handle);
    CHECK_ROCJPEG(status);
    py::capsule capsule(
        decode_handle,
        "RocJpegHandle",
        [](PyObject *capsule) {
            auto ptr = static_cast<RocJpegHandle>(PyCapsule_GetPointer(capsule, "RocJpegHandle"));
            rocJpegDestroy(ptr);
        }
    );
    return py::make_tuple(capsule, status);
}

py::tuple PyRocJpegDecoder::rocPyJpegGetImageInfo(py::capsule decode_handle, py::capsule stream_handle) {
    uint8_t num_components = 0;
    RocJpegChromaSubsampling subsampling;
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> m_widths = {};
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> m_heights = {};
    auto* jpeg_stream_handle = static_cast<RocJpegStreamHandle>(stream_handle.get_pointer());
    auto* jpeg_decode_handle = static_cast<RocJpegHandle>(decode_handle.get_pointer());
    RocJpegStatus status = rocJpegGetImageInfo(jpeg_decode_handle, jpeg_stream_handle, &num_components, &subsampling, m_widths.data(), m_heights.data());
    CHECK_ROCJPEG(status);
    return py::make_tuple(subsampling, m_widths, m_heights, status);
}


py::object PyRocJpegDecoder::rocPyJpegDecode(PyRocJpegDecodeParams &in_decode_params, py::capsule decode_handle, py::capsule stream_handle, py::capsule image_ptr) {
    RocJpegImage* output_image = static_cast<RocJpegImage*>(image_ptr.get_pointer());
    RocJpegDecodeParams decode_params;
    memcpy(&decode_params,&in_decode_params,sizeof(RocJpegDecodeParams));
    RocJpegStreamHandle jpeg_stream_handle = stream_handle.get_pointer();
    RocJpegHandle jpeg_decode_handle = decode_handle.get_pointer();
    RocJpegStatus status = rocJpegDecode(jpeg_decode_handle, jpeg_stream_handle, &decode_params, output_image);
    CHECK_ROCJPEG(status);
    return py::cast(status);
}

py::object PyRocJpegDecoder::rocPyJpegDecodeBatched(
    py::capsule decode_handle_capsule,
    py::list stream_capsules,
    int batch_size,
    py::list in_decode_params,
    py::capsule destinations_capsule
) {
    auto* decode_handle = static_cast<RocJpegHandle>(decode_handle_capsule.get_pointer());
    // Extract stream handles
    std::vector<RocJpegStreamHandle> stream_handles;
    for (auto item : stream_capsules) {
        py::capsule stream_capsule = py::cast<py::capsule>(item);
        auto* stream_handle = static_cast<RocJpegStreamHandle>(stream_capsule.get_pointer());
        stream_handles.push_back(stream_handle);
    }
    auto* destinations = static_cast<RocJpegImage*>(destinations_capsule.get_pointer());
    RocJpegDecodeParams decode_params;
    // Extract decode parameters
    std::vector<RocJpegDecodeParams> decode_params_list;
    for (auto item : in_decode_params) {
        PyRocJpegDecodeParams py_decode_param = py::cast<PyRocJpegDecodeParams>(item);
        memcpy(&decode_params,&py_decode_param,sizeof(RocJpegDecodeParams));
        decode_params_list.push_back(decode_params);
    }
    // Call C API for batched decoding with decode_params_list.data()
    RocJpegStatus status = rocJpegDecodeBatched(
        decode_handle,
        stream_handles.data(),
        batch_size,
        decode_params_list.data(),
        destinations
    );
    CHECK_ROCJPEG(status);
    return py::cast(status);
}


//
// Utils
//

std::string
PyRocJpegUtils::PyGetOutputFileExt(PyRocJpegDecodeParams &in_decode_params, std::string &base_file_name, uint32_t image_width, uint32_t image_height, RocJpegChromaSubsampling subsampling, std::string &image_save_path) {
    std::string file_name_for_saving = image_save_path;
    GetOutputFileExt(in_decode_params.output_format, base_file_name, image_width, image_height, subsampling, file_name_for_saving);
    return file_name_for_saving.c_str();
}

py::object
PyRocJpegUtils::PySaveImage(PyRocJpegDecodeParams &in_decode_params, std::string image_save_path, uint32_t img_width, uint32_t img_height, RocJpegChromaSubsampling subsampling, py::capsule image_ptr) {
    RocJpegImage* output_image = static_cast<RocJpegImage*>(image_ptr.get_pointer());
    SaveImage(image_save_path, output_image, img_width, img_height, subsampling, in_decode_params.output_format);
    return py::cast<py::none>(Py_None);
}

py::tuple PyRocJpegUtils::PyGetFilePaths(std::string &input_path) {
    bool is_dir = false, is_file = false;
    std::vector<std::string> file_paths;
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    if (!GetFilePaths(input_path, file_paths, is_dir, is_file)) {
        std::cerr << "ERROR: Failed to get input file paths!" << std::endl;
        status = ROCJPEG_STATUS_RUNTIME_ERROR;
    }
    return py::make_tuple(file_paths, is_dir, status);
}


py::tuple PyRocJpegUtils::PyGetChannelPitchAndSizes(
    PyRocJpegDecodeParams &in_decode_params,
    RocJpegChromaSubsampling subsampling,
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &widths,
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &heights,
    py::capsule image_ptr) {
    RocJpegImage* output_image = static_cast<RocJpegImage*>(image_ptr.get_pointer());
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> channel_sizes = {};
    uint32_t num_channels = 0;
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    RocJpegDecodeParams decode_params;
    memcpy(&decode_params, &in_decode_params, sizeof(RocJpegDecodeParams));
    bool ret = GetChannelPitchAndSizes(decode_params, subsampling, widths.data(), heights.data(), num_channels, *output_image, reinterpret_cast<uint32_t*>(&channel_sizes));
    if (ret != EXIT_SUCCESS) {
        num_channels = 0;
        status = ROCJPEG_STATUS_RUNTIME_ERROR;
    }
    return py::make_tuple(num_channels, channel_sizes, status);
}


py::tuple PyRocJpegUtils::PyGetChromaSubsamplingStr(RocJpegChromaSubsampling subsampling) {
    std::string chroma_sub_sampling;
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    switch (subsampling) {
        case ROCJPEG_CSS_444:
            chroma_sub_sampling = "YUV 4:4:4";
            break;
        case ROCJPEG_CSS_440:
            chroma_sub_sampling = "YUV 4:4:0";
            break;
        case ROCJPEG_CSS_422:
            chroma_sub_sampling = "YUV 4:2:2";
            break;
        case ROCJPEG_CSS_420:
            chroma_sub_sampling = "YUV 4:2:0";
            break;
        case ROCJPEG_CSS_411:
            chroma_sub_sampling = "YUV 4:1:1";
            break;
        case ROCJPEG_CSS_400:
            chroma_sub_sampling = "YUV 4:0:0";
            break;
        case ROCJPEG_CSS_UNKNOWN:
            chroma_sub_sampling = "UNKNOWN";
            break;
        default:
            chroma_sub_sampling = "";
            status = ROCJPEG_STATUS_RUNTIME_ERROR;
            break;
    }
    return py::make_tuple(chroma_sub_sampling, status);
}


py::object PyRocJpegUtils::PyInitHipDevice(int device_id) {
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    if (!InitHipDevice(device_id)) {
        std::cerr << "ERROR: Failed to initialize HIP!" << std::endl;
        status = ROCJPEG_STATUS_RUNTIME_ERROR;
    }
    return py::cast(status);
}

py::tuple PyRocJpegUtils::PyAllocHipDeviceMemory(uint32_t &channel_size) {
    // allocate device memory
    uint8_t *output_image_channel = nullptr;
    RocJpegStatus status = static_cast<RocJpegStatus>(hipMalloc((void **)&output_image_channel, channel_size));
    uintptr_t output_ptr = reinterpret_cast<uintptr_t>(output_image_channel);
    return py::make_tuple(output_ptr, status);
}

py::object PyRocJpegUtils::PyFreeHipDeviceMemory(uintptr_t output_image_channel) {
    // Free the allocated memory
    uint8_t *chn_adrs = reinterpret_cast<uint8_t*>(output_image_channel);
    RocJpegStatus status = static_cast<RocJpegStatus>(hipFree((void *)chn_adrs));
    return py::cast(status);
}
