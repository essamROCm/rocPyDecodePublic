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
        .def("PyGetChromaSubsamplingStr",&PyRocJpegDecoder::PyGetChromaSubsamplingStr)
        .def("PyInitDecodeParams",&PyRocJpegDecoder::PyInitDecodeParams)
        .def("PyGetChannelPitchAndSizes",&PyRocJpegDecoder::PyGetChannelPitchAndSizes)
        .def("rocPyAllocHipDeviceMemory",&PyRocJpegDecoder::rocPyAllocHipDeviceMemory)
        .def("rocPyJpegDecode",&PyRocJpegDecoder::rocPyJpegDecode)
        .def("rocPyFreeHipDeviceMemory",&PyRocJpegDecoder::rocPyFreeHipDeviceMemory)
        .def("jpeg_print_variables",&PyRocJpegDecoder::jpeg_print_variables)
        .def("PyGetOutputFileExt",&PyRocJpegDecoder::PyGetOutputFileExt)
        .def("PySaveImage",&PyRocJpegDecoder::PySaveImage)
        ;
}

PyRocJpegDecoder::PyRocJpegDecoder() {
    m_decode_params.output_format = ROCJPEG_OUTPUT_NATIVE;
    m_decode_params.crop_rectangle.left = 0;
    m_decode_params.crop_rectangle.top = 0;
    m_decode_params.crop_rectangle.right = 0;
    m_decode_params.crop_rectangle.bottom = 0;
    m_decode_params.target_dimension.width = 0;
    m_decode_params.target_dimension.height = 0;

    memset(&m_output_image.channel, 0, ROCJPEG_MAX_COMPONENT * sizeof(uint8_t*));
    memset(&m_output_image.pitch, 0, ROCJPEG_MAX_COMPONENT * sizeof(uint32_t));

    memset(&m_widths, 0, ROCJPEG_MAX_COMPONENT * sizeof(uint32_t));
    memset(&m_heights, 0, ROCJPEG_MAX_COMPONENT * sizeof(uint32_t));
    memset(&m_channel_sizes, 0, ROCJPEG_MAX_COMPONENT * sizeof(uint32_t));
    memset(&m_prior_channel_sizes, 0, ROCJPEG_MAX_COMPONENT * sizeof(uint32_t));
}

PyRocJpegDecoder::~PyRocJpegDecoder() {

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
    CHECK_ROCJPEG(rocJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handle, &num_components, &subsampling, m_widths, m_heights));
    return std::make_tuple(num_components, subsampling, m_widths[0], m_heights[0]);
}

py::object PyRocJpegDecoder::rocPyJpegDecode(RocJpegHandle rocjpeg_handle, RocJpegStreamHandle rocjpeg_stream_handle) {
    CHECK_ROCJPEG(rocJpegDecode(rocjpeg_handle, rocjpeg_stream_handle, &m_decode_params, &m_output_image));
    return py::cast<py::none>(Py_None);
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

py::object PyRocJpegDecoder::rocPyAllocHipDeviceMemory(int num_channels) {
    // allocate memory for each channel and reuse them if the sizes remain unchanged for a new image.
    for (int i = 0; i < num_channels; i++) {
        if (m_prior_channel_sizes[i] != m_channel_sizes[i]) {
            if (m_output_image.channel[i] != nullptr) {
                CHECK_HIP(hipFree((void *)m_output_image.channel[i]));
                m_output_image.channel[i] = nullptr;
            }
            CHECK_HIP(hipMalloc(&m_output_image.channel[i], m_channel_sizes[i]));
        }
    }
    for (int i = 0; i < ROCJPEG_MAX_COMPONENT; i++) {
        m_prior_channel_sizes[i] = m_channel_sizes[i];
    }
    return py::cast<py::none>(Py_None);
}

py::object PyRocJpegDecoder::rocPyFreeHipDeviceMemory(int num_channels) {
    // Free the allocated memory for each channel
    for (int i = 0; i < num_channels; i++) {
        if (m_output_image.channel[i] != nullptr) {
            CHECK_HIP(hipFree((void *)m_output_image.channel[i]));
            m_output_image.channel[i] = nullptr;
        }
    }
    return py::cast<py::none>(Py_None);
}

std::string
PyRocJpegDecoder::PyGetOutputFileExt(std::string &base_file_name, uint32_t image_width, uint32_t image_height, RocJpegChromaSubsampling subsampling, std::string &image_save_path) {
    std::string file_name_for_saving = image_save_path;
    GetOutputFileExt(m_decode_params.output_format, base_file_name, image_width, image_height, subsampling, file_name_for_saving);
    return file_name_for_saving.c_str();
}

py::object
PyRocJpegDecoder::PySaveImage(std::string image_save_path, uint32_t img_width, uint32_t img_height, RocJpegChromaSubsampling subsampling) {
    SaveImage(image_save_path, &m_output_image, img_width, img_height, subsampling, m_decode_params.output_format);
    return py::cast<py::none>(Py_None);
}

std::string
PyRocJpegDecoder::PyGetChromaSubsamplingStr(RocJpegChromaSubsampling subsampling) {
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

py::int_ PyRocJpegDecoder::PyGetChannelPitchAndSizes(RocJpegChromaSubsampling subsampling) {
    uint32_t num_channels=0;
    bool ret = GetChannelPitchAndSizes(m_decode_params, subsampling, m_widths, m_heights, num_channels, m_output_image, m_channel_sizes);
    if(ret != EXIT_SUCCESS)
        num_channels = 0;
    return py::int_(static_cast<int>(num_channels));
}

std::tuple<int, int>
PyRocJpegDecoder::PyInitDecodeParams(int output_format, int left, int top, int right, int bottom) {
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

// TODO: remove in final version of the PR; DEBUG helper
void PyRocJpegDecoder::jpeg_print_variables() {
    // Print m_decode_params
    std::cout << "\n=== m_decode_params ===" << std::endl;
    std::cout << "Output Format: " << static_cast<int>(m_decode_params.output_format) << std::endl;
    std::cout << "Crop Rectangle: Left=" << m_decode_params.crop_rectangle.left
              << ", Top=" << m_decode_params.crop_rectangle.top
              << ", Right=" << m_decode_params.crop_rectangle.right
              << ", Bottom=" << m_decode_params.crop_rectangle.bottom << std::endl;
    std::cout << "Target Dimension: Width=" << m_decode_params.target_dimension.width
              << ", Height=" << m_decode_params.target_dimension.height << std::endl;
    // Print m_widths
    std::cout << "\n=== m_widths ===" << std::endl;
    for (int i = 0; i < ROCJPEG_MAX_COMPONENT; i++) {
        std::cout << "Width[" << i << "]: " << m_widths[i] << std::endl;
    }
    // Print m_heights
    std::cout << "\n=== m_heights ===" << std::endl;
    for (int i = 0; i < ROCJPEG_MAX_COMPONENT; i++) {
        std::cout << "Height[" << i << "]: " << m_heights[i] << std::endl;
    }
    // Print m_channel_sizes
    std::cout << "\n=== m_channel_sizes ===" << std::endl;
    for (int i = 0; i < ROCJPEG_MAX_COMPONENT; i++) {
        std::cout << "Channel Size[" << i << "]: " << m_channel_sizes[i] << std::endl;
    }
    // Print m_prior_channel_sizes
    std::cout << "\n=== m_prior_channel_sizes ===" << std::endl;
    for (int i = 0; i < ROCJPEG_MAX_COMPONENT; i++) {
        std::cout << "Prior Channel Size[" << i << "]: " << m_prior_channel_sizes[i] << std::endl;
    }
    // Print m_output_image
    std::cout << "\n=== m_output_image ===" << std::endl;
    for (int i = 0; i < ROCJPEG_MAX_COMPONENT; i++) {
        std::cout << "Channel[" << i << "] Address: " << static_cast<void*>(m_output_image.channel[i]) << std::endl;
        std::cout << "Pitch[" << i << "]: " << m_output_image.pitch[i] << std::endl;
    }
}
