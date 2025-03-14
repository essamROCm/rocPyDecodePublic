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
        .def("rocPyJpegDecode",&PyRocJpegDecoder::rocPyJpegDecode);
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

py::capsule PyRocJpegDecoder::rocPyJpegStreamCreate() {
    RocJpegStreamHandle rocjpeg_stream_handle = nullptr;
    CHECK_ROCJPEG(rocJpegStreamCreate(&rocjpeg_stream_handle));

    // Wrap handle in capsule with destructor
    return py::capsule(
        rocjpeg_stream_handle,                  // Raw pointer
        "RocJpegStreamHandle",                  // Capsule name for type checking
        (void (*)(void *))( [](void *ptr) {     // Destructor with explicit cast
            rocJpegStreamDestroy(static_cast<RocJpegStreamHandle>(ptr));
        })
    );
}

py::object PyRocJpegDecoder::rocPyJpegStreamParse(py::array_t<uint8_t> file_data, size_t length, py::capsule stream_handle) {
    auto buf = file_data.request();
    uint8_t* ptr = static_cast<uint8_t*>(buf.ptr);
    RocJpegStreamHandle jpeg_stream_handle = stream_handle.get_pointer();
    RocJpegStatus status = rocJpegStreamParse(ptr, length, jpeg_stream_handle);
    return py::cast(status);
}

py::capsule PyRocJpegDecoder::rocPyJpegCreate(RocJpegBackend backend, int device_id) {
    RocJpegHandle decode_handle = nullptr;
    CHECK_ROCJPEG(rocJpegCreate(backend, device_id, &decode_handle));

    // Wrap RocJpegHandle as capsule with destructor
    return py::capsule(
        decode_handle,                         // Raw C pointer
        "RocJpegHandle",                        // Capsule name (for type-checking in other functions)
        (void (*)(void *))( [](void *ptr) {     // Explicit destructor cast
            rocJpegDestroy(static_cast<RocJpegHandle>(ptr));  // Call cleanup when capsule is destroyed
        })
    );
}

std::tuple<uint8_t, RocJpegChromaSubsampling, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>>
PyRocJpegDecoder::rocPyJpegGetImageInfo(py::capsule decode_handle, py::capsule stream_handle) {
    uint8_t num_components=0;
    RocJpegChromaSubsampling subsampling;
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> m_widths = {};
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> m_heights = {};
    RocJpegStreamHandle jpeg_stream_handle = stream_handle.get_pointer();
    RocJpegHandle jpeg_decode_handle = decode_handle.get_pointer();
    CHECK_ROCJPEG(rocJpegGetImageInfo(jpeg_decode_handle, jpeg_stream_handle, &num_components, &subsampling, m_widths.data(), m_heights.data()));
    return std::make_tuple(num_components, subsampling, m_widths, m_heights);
}

py::object PyRocJpegDecoder::rocPyJpegDecode(PyRocJpegDecodeParams &in_decode_params, py::capsule decode_handle, py::capsule stream_handle, void *image_ptr) {
    RocJpegImage* output_image = static_cast<RocJpegImage*>(image_ptr);
    RocJpegDecodeParams decode_params = get_decode_param(in_decode_params);
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
    PyRocJpegDecodeParams& in_decode_params,
    py::capsule destinations_capsule
) {
    auto* decode_handle = static_cast<RocJpegHandle>(decode_handle_capsule.get_pointer());

    // Extract stream handles
    std::vector<RocJpegStreamHandle> stream_handles;
    for (auto item : stream_capsules) {
        py::capsule stream_capsule = py::cast<py::capsule>(item);
        if (std::string(stream_capsule.name()) != "RocJpegStreamHandle") {
            throw std::runtime_error("Invalid stream handle in list");
        }
        auto* stream_handle = static_cast<RocJpegStreamHandle>(stream_capsule.get_pointer());
        stream_handles.push_back(stream_handle);
    }

    if (stream_handles.size() != static_cast<size_t>(batch_size)) {
        throw std::runtime_error("Batch size does not match number of stream handles provided");
    }

    auto* destinations = static_cast<RocJpegImage*>(destinations_capsule.get_pointer());

    RocJpegDecodeParams decode_params = get_decode_param(in_decode_params);

    // Call C API for batched decoding
    return py::cast(rocJpegDecodeBatched(
        decode_handle,
        stream_handles.data(),
        batch_size,
        &decode_params,
        destinations
    ));
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
PyRocJpegUtils::PySaveImage(PyRocJpegDecodeParams &in_decode_params, std::string image_save_path, uint32_t img_width, uint32_t img_height, RocJpegChromaSubsampling subsampling, void *image_ptr) {
    RocJpegImage* output_image = static_cast<RocJpegImage*>(image_ptr);
    SaveImage(image_save_path, output_image, img_width, img_height, subsampling, in_decode_params.output_format);
    return py::cast<py::none>(Py_None);
}

std::tuple<std::string, std::vector<std::string>, bool, bool>
PyRocJpegUtils::PyGetFilePaths(std::string &input_path, std::vector<std::string> &file_paths, bool &is_dir, bool &is_file) {
    is_dir = is_file = false;
    if (!GetFilePaths(input_path, file_paths, is_dir, is_file)) {
        std::cerr << "ERROR: Failed to get input file paths!" << std::endl;
    }
    return std::make_tuple(input_path, file_paths, is_dir, is_file);
}

std::tuple<int, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>>
PyRocJpegUtils::PyGetChannelPitchAndSizes(PyRocJpegDecodeParams &in_decode_params, RocJpegChromaSubsampling subsampling, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &widths,
                    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &heights, void *image_ptr) {
    RocJpegImage* output_image = static_cast<RocJpegImage*>(image_ptr);
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> channel_sizes = {};
    uint32_t num_channels=0;
    RocJpegDecodeParams decode_params = get_decode_param(in_decode_params);
    bool ret = GetChannelPitchAndSizes(decode_params, subsampling, widths.data(), heights.data(), num_channels, *output_image, reinterpret_cast<uint32_t *>(&channel_sizes));
    if(ret != EXIT_SUCCESS)
        num_channels = 0;
    return std::make_tuple(num_channels, channel_sizes);
}

std::string
PyRocJpegUtils::PyGetChromaSubsamplingStr(RocJpegChromaSubsampling subsampling) {
    std::string chroma_sub_sampling;
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
            break;
    }
    std::cout << "Chroma String: " << chroma_sub_sampling << std::endl;
    return chroma_sub_sampling.c_str();
}

py::object PyRocJpegUtils::PyInitHipDevice(int device_id) {
    bool ret = true;
    if (!(ret=InitHipDevice(device_id))) {
        std::cerr << "ERROR: Failed to initialize HIP!" << std::endl;
    }
    return py::cast(ret);
}

std::tuple<RocJpegStatus,uintptr_t>
PyRocJpegUtils::PyAllocHipDeviceMemory(uint32_t &channel_size) {
    // allocate device memory
    uint8_t *output_image_channel = nullptr;
    RocJpegStatus status = static_cast<RocJpegStatus>(hipMalloc((void **)&output_image_channel, channel_size));
    uintptr_t output_ptr = reinterpret_cast<uintptr_t>(output_image_channel);
    return std::make_tuple(status, output_ptr);
}

py::object PyRocJpegUtils::PyFreeHipDeviceMemory(uintptr_t output_image_channel) {
    // Free the allocated memory
    uint8_t *chn_adrs = reinterpret_cast<uint8_t*>(output_image_channel);
    RocJpegStatus status = static_cast<RocJpegStatus>(hipFree((void *)chn_adrs));
    return py::cast(status);
}