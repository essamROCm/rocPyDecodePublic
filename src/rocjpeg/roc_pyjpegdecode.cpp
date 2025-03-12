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
        .def("rocPyJpegDestroy",&PyRocJpegDecoder::rocPyJpegDestroy)
        .def("rocPyJpegStreamDestroy",&PyRocJpegDecoder::rocPyJpegStreamDestroy)
        .def("rocPyJpegStreamParse",&PyRocJpegDecoder::rocPyJpegStreamParse)
        .def("rocPyJpegGetImageInfo",&PyRocJpegDecoder::rocPyJpegGetImageInfo)
        .def("PyInitHipDevice",&PyRocJpegDecoder::PyInitHipDevice)
        .def("rocPyAllocHipDeviceMemory",&PyRocJpegDecoder::rocPyAllocHipDeviceMemory)
        .def("rocPyFreeHipDeviceMemory",&PyRocJpegDecoder::rocPyFreeHipDeviceMemory)
        .def("rocPyJpegDecode",&PyRocJpegDecoder::rocPyJpegDecode)
        .def("rocPyInitDecodeParams",&PyRocJpegDecoder::rocPyInitDecodeParams);
}

void PyRocJpegUtilsInitializer(py::module& m) {
    py::class_<PyRocJpegUtils> (m, "PyRocJpegUtils")
        .def(py::init<>())
        // Utils
        .def("PyGetOutputFileExt",&PyRocJpegUtils::PyGetOutputFileExt)
        .def("PySaveImage",&PyRocJpegUtils::PySaveImage)
        .def("PyGetFilePaths",&PyRocJpegUtils::PyGetFilePaths)
        .def("PyGetChannelPitchAndSizes",&PyRocJpegUtils::PyGetChannelPitchAndSizes)
        .def("PyGetChromaSubsamplingStr",&PyRocJpegUtils::PyGetChromaSubsamplingStr);
}

//
// rocPyJPEG APIs
//

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

std::tuple<uint8_t, RocJpegChromaSubsampling, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>>
PyRocJpegDecoder::rocPyJpegGetImageInfo(RocJpegHandle rocjpeg_handle, RocJpegStreamHandle rocjpeg_stream_handle) {
    uint8_t num_components=0;
    RocJpegChromaSubsampling subsampling;
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> m_widths = {};
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> m_heights = {};
    CHECK_ROCJPEG(rocJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handle, &num_components, &subsampling, m_widths.data(), m_heights.data()));
    return std::make_tuple(num_components, subsampling, m_widths, m_heights);
}

PyRocJpegImage PyRocJpegDecoder::rocPyJpegDecode(PyRocJpegDecodeParams &m_decode_params, RocJpegHandle rocjpeg_handle, RocJpegStreamHandle rocjpeg_stream_handle, PyRocJpegImage& py_img) {
    RocJpegImage output_image = py_img.to_c_struct();
    RocJpegDecodeParams decode_params;
    memcpy(&decode_params, &m_decode_params, sizeof(RocJpegDecodeParams));
    CHECK_ROCJPEG(rocJpegDecode(rocjpeg_handle, rocjpeg_stream_handle, &decode_params, &output_image));
    py_img.from_c_struct(output_image);
    return py_img;
}

py::object PyRocJpegDecoder::rocPyJpegDecodeBatched(RocJpegHandle handle, RocJpegStreamHandle *jpeg_stream_handles, int batch_size, const PyRocJpegDecodeParams *m_decode_params, RocJpegImage *destinations) {
    RocJpegDecodeParams decode_params;
    memcpy(&decode_params, &m_decode_params, sizeof(RocJpegDecodeParams));
    return py::cast(rocJpegDecodeBatched(handle, jpeg_stream_handles, batch_size, &decode_params, destinations));
}

PyRocJpegImage PyRocJpegDecoder::rocPyAllocHipDeviceMemory(int num_channels, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &channel_sizes, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &prior_channel_sizes, PyRocJpegImage& py_img) {
    RocJpegImage output_image = py_img.to_c_struct();
    // allocate memory for each channel and reuse them if the sizes remain unchanged for a new image.
    for (int i = 0; i < num_channels; i++) {
        if (prior_channel_sizes[i] != channel_sizes[i]) {
            if (output_image.channel[i] != nullptr) {
                CHECK_HIP(hipFree((void *)output_image.channel[i]));
                output_image.channel[i] = nullptr;
            }
            CHECK_HIP(hipMalloc(&output_image.channel[i], channel_sizes[i]));
        }
    }
    py_img.from_c_struct(output_image);
    return py_img;
}

PyRocJpegImage PyRocJpegDecoder::rocPyFreeHipDeviceMemory(int num_channels, PyRocJpegImage& py_img) {
    RocJpegImage output_image = py_img.to_c_struct();
    // Free the allocated memory for each channel
    for (int i = 0; i < num_channels; i++) {
        if (output_image.channel[i] != nullptr) {
            CHECK_HIP(hipFree((void *)output_image.channel[i]));
            output_image.channel[i] = nullptr;
        }
    }
    py_img.from_c_struct(output_image);
    return py_img;
}

py::object PyRocJpegDecoder::PyInitHipDevice(int device_id) {
    bool ret = true;
    if (!(ret=InitHipDevice(device_id))) {
        std::cerr << "ERROR: Failed to initialize HIP!" << std::endl;
    }
    return py::cast(ret);
}

std::tuple<int, int>
PyRocJpegDecoder::rocPyInitDecodeParams(PyRocJpegDecodeParams &m_decode_params, int output_format, int left, int top, int right, int bottom) {
    // format [1:native, 2:yuv_planar, 3:y, 4:rgb, 5:rgb_planar]
    switch(output_format) {
        case 2:
            m_decode_params.output_format = ROCJPEG_OUTPUT_YUV_PLANAR;
            break;
        case 3:
            m_decode_params.output_format = ROCJPEG_OUTPUT_Y;
            break;
        case 4:
            m_decode_params.output_format = ROCJPEG_OUTPUT_RGB;
            break;
        case 5:
            m_decode_params.output_format = ROCJPEG_OUTPUT_RGB_PLANAR;
            break;
        default:
            m_decode_params.output_format = ROCJPEG_OUTPUT_NATIVE;
            break;
    }
    // crop
    m_decode_params.crop_rectangle.left = left;
    m_decode_params.crop_rectangle.top = top;
    m_decode_params.crop_rectangle.right = right;
    m_decode_params.crop_rectangle.bottom = bottom;
    // roi
    int roi_width = 0, roi_height = 0;
    roi_width = m_decode_params.crop_rectangle.right - m_decode_params.crop_rectangle.left;
    roi_height = m_decode_params.crop_rectangle.bottom - m_decode_params.crop_rectangle.top;
    return std::make_tuple(roi_width, roi_height);
}

//
// Utils
//

std::string
PyRocJpegUtils::PyGetOutputFileExt(PyRocJpegDecodeParams &m_decode_params, std::string &base_file_name, uint32_t image_width, uint32_t image_height, RocJpegChromaSubsampling subsampling, std::string &image_save_path) {
    std::string file_name_for_saving = image_save_path;
    GetOutputFileExt(m_decode_params.output_format, base_file_name, image_width, image_height, subsampling, file_name_for_saving);
    return file_name_for_saving.c_str();
}

py::object
PyRocJpegUtils::PySaveImage(PyRocJpegDecodeParams &m_decode_params, std::string image_save_path, uint32_t img_width, uint32_t img_height, RocJpegChromaSubsampling subsampling, PyRocJpegImage& py_img) {
    RocJpegImage output_image = py_img.to_c_struct();
    SaveImage(image_save_path, &output_image, img_width, img_height, subsampling, m_decode_params.output_format);
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

std::tuple<int, std::array<uint32_t, ROCJPEG_MAX_COMPONENT>, PyRocJpegImage>
PyRocJpegUtils::PyGetChannelPitchAndSizes(PyRocJpegDecodeParams &m_decode_params, RocJpegChromaSubsampling subsampling,
                            std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &widths, std::array<uint32_t, ROCJPEG_MAX_COMPONENT> &heights, PyRocJpegImage& py_img) {
    RocJpegImage output_image = py_img.to_c_struct();
    std::array<uint32_t, ROCJPEG_MAX_COMPONENT> channel_sizes = {};
    uint32_t num_channels=0;
    RocJpegDecodeParams decode_params;
    memcpy(&decode_params, &m_decode_params, sizeof(RocJpegDecodeParams));
    bool ret = GetChannelPitchAndSizes(decode_params, subsampling, widths.data(), heights.data(), num_channels, output_image, reinterpret_cast<uint32_t *>(&channel_sizes));
    if(ret != EXIT_SUCCESS)
        num_channels = 0;
    py_img.from_c_struct(output_image);
    return std::make_tuple(num_channels, channel_sizes, py_img);
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
