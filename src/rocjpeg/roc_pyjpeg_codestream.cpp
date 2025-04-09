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

#include "roc_pyjpeg.h"
#include "roc_pyjpeg_decoder.h"
#include "roc_pyjpeg_buffer.h"
#include "roc_pyjpeg_codestream.h"
#include <algorithm>     // for std::copy

using namespace std;

void CodeStream::exportToPython(py::module& m) {
    // clang-format off
    py::class_<CodeStream>(m, "CodeStream",
        R"pbdoc(
        Class representing a coded stream of image data.

        This class provides access to image informations such as dimensions.
        It supports initialization from bytes, numpy arrays, or file path.
        )pbdoc")
        .def(py::init([](py::bytes bytes) {
            return new CodeStream(bytes);
            }),
            "bytes"_a, py::keep_alive<1, 2>(),
            R"pbdoc(
            Initialize a CodeStream using bytes as input.

            Args:
                bytes: The byte data representing the encoded stream.
            )pbdoc")
        .def(py::init([](py::array_t<uint8_t> arr) {
            return new CodeStream(arr);
            }),
            "array"_a, py::keep_alive<1, 2>(),
            R"pbdoc(
            Initialize a CodeStream using a numpy array of uint8 as input.

            Args:
                array: The numpy array containing the encoded stream.
            )pbdoc")
        .def(py::init([](const std::filesystem::path& filename) {
            return new CodeStream(filename);
            }),
            "filename"_a, py::keep_alive<1, 2>(),
            R"pbdoc(
            Initialize a CodeStream using a file path as input.

            Args:
                filename: The file path to the encoded stream data.
            )pbdoc")
        .def_property_readonly("height", &CodeStream::height, 
            R"pbdoc(
            The vertical dimension of the entire image in pixels.
            )pbdoc")
        .def_property_readonly("width", &CodeStream::width, 
            R"pbdoc(
            The horizontal dimension of the entire image in pixels.
            )pbdoc")
            ;
}

CodeStream* CodeStream::handle() {
    return this;
}

CodeStream& CodeStream::operator=(const CodeStream& other) {
    if (this == &other)
        return *this;  // self-assignment guard
    // Copy public members
    stream_handle = other.stream_handle;
    decode_params = other.decode_params;
    output_image = other.output_image;
    subsampling = other.subsampling;
    std::copy(std::begin(other.widths), std::end(other.widths), std::begin(widths));
    std::copy(std::begin(other.heights), std::end(other.heights), std::begin(heights));
    std::copy(std::begin(other.channel_sizes), std::end(other.channel_sizes), std::begin(channel_sizes));
    num_channels = other.num_channels;
    code_stream_ = other.code_stream_;  // raw pointer copy (or convert to shared_ptr if needed)
    // Copy private members
    data_ref_bytes_ = other.data_ref_bytes_;
    data_ref_arr_ = other.data_ref_arr_;
    m_width = other.m_width;
    m_height = other.m_height;
    return *this;
}

int CodeStream::ReadImageFromDiskFile(const std::filesystem::path& filename, std::vector<char>& file_data, int& file_size) {
    // Read an image from disk.
    std::ifstream input(filename.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    if (!(input.is_open())) {
        std::cerr << "ERROR: Cannot open image: " << filename << std::endl;
        return EXIT_FAILURE;
    }
    // Get the size
    file_size = input.tellg();
    input.seekg(0, std::ios::beg);
    // resize if buffer is too small
    if (file_data.size() < file_size) {
        file_data.resize(file_size);
    }
    if (!input.read(file_data.data(), file_size)) {
        std::cerr << "ERROR: Cannot read from file: " << filename << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Use the dat and its size if valid, otherwise use the file to load the data
int CodeStream::PrepareStreamForOneImageDecoding(const std::filesystem::path& filename, const unsigned char* data, int data_size) {
    // File sanity chek
    if(!filename.empty()) {
        std::cout << "Input file name: " << filename << std::endl;
        if(!std::filesystem::exists(filename)) {
            std::cerr << "Invalid or missing file: " << filename << std::endl;
        }
    }
    // Read file data if no data sent
    std::vector<char> file_data;
    int file_size = data_size;
    if (data != nullptr && data_size > 0) {
        file_data.resize(data_size);
        file_data.assign(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + data_size);
    }    
    if(data == nullptr) {
        int ret = EXIT_SUCCESS;
        if((ret = ReadImageFromDiskFile(filename, file_data, file_size)) != EXIT_SUCCESS)
            return ret;
    }

    // Create Stream
    PY_CHECK_ROCJPEG(rocJpegStreamCreate(&stream_handle));

    // Stream Parse
    RocJpegStatus rocjpeg_status = rocJpegStreamParse(reinterpret_cast<uint8_t*>(file_data.data()), file_size, stream_handle);
    if (rocjpeg_status != ROCJPEG_STATUS_SUCCESS) {
        std::cerr << "ERROR: Failed to parse the input jpeg stream with " << rocJpegGetErrorName(rocjpeg_status) << std::endl;
        return EXIT_FAILURE;
    }

    // Get Image Info
    uint8_t num_components = 0;
    subsampling = ROCJPEG_CSS_UNKNOWN;
    PY_CHECK_ROCJPEG(rocJpegGetImageInfo(rocjpeg_handle, stream_handle, &num_components, &subsampling, widths, heights));

    // Check limits of w/h & subsampling
    PyRocJpegUtils rocjpeg_utils;
    std::string chroma_sub_sampling = "";
    rocjpeg_utils.GetChromaSubsamplingStr(subsampling, chroma_sub_sampling);
    std::cout << "Input image resolution: " << widths[0] << "x" << heights[0] << std::endl;
    std::cout << "Chroma subsampling STR: " + chroma_sub_sampling  << std::endl;
    std::cout << "Chroma subsampling INT: " << static_cast<int>(subsampling)  << std::endl;
    if (widths[0] < 64 || heights[0] < 64) {
        std::cerr << "The image resolution is not supported by VCN Hardware" << std::endl;
        return EXIT_FAILURE;
    }
    if (subsampling == ROCJPEG_CSS_411 || subsampling == ROCJPEG_CSS_UNKNOWN) {
        std::cerr << "The chroma sub-sampling is not supported by VCN Hardware" << std::endl;
        return EXIT_FAILURE;
    }    

    // save to inner store
    m_width = widths[0];
    m_height = heights[0];

    // Get Channel Pitch And Sizes
    memset(&decode_params, 0, sizeof(RocJpegDecodeParams));
    memset(&output_image, 0, sizeof(RocJpegImage));    
    if (rocjpeg_utils.GetChannelPitchAndSizes(decode_params, subsampling, widths, heights, num_channels, output_image, channel_sizes)) {
        std::cerr << "ERROR: Failed to get the channel pitch and sizes" << std::endl;
        return EXIT_FAILURE;
    }

    // allocate memory for each channel
    for (int i = 0; i < num_channels; i++) {
            if (output_image.channel[i] != nullptr) {
                PY_CHECK_HIP(hipFree((void *)output_image.channel[i]));
                output_image.channel[i] = nullptr;
            }
            PY_CHECK_HIP(hipMalloc(&output_image.channel[i], channel_sizes[i]));
    }
    return EXIT_SUCCESS;
}

CodeStream::CodeStream(const std::filesystem::path& filename) {
    PY_CHECK_DECODER();
    // code stream from one file
    PrepareStreamForOneImageDecoding(filename, nullptr, 0);
}

CodeStream::CodeStream(const unsigned char* data, size_t length) {
    PY_CHECK_DECODER();
    // code stream from one file
    PrepareStreamForOneImageDecoding(static_cast<const std::filesystem::path>(""), data, length);
}

CodeStream::CodeStream(py::bytes data) {
    PY_CHECK_DECODER();
    data_ref_bytes_ = data;
    auto data_view = static_cast<std::string_view>(data_ref_bytes_);
    py::gil_scoped_release release;
    // code stream from one file
    PrepareStreamForOneImageDecoding(static_cast<const std::filesystem::path>(""), reinterpret_cast<const unsigned char*>(data_view.data()), data_view.size());
}

CodeStream::CodeStream(py::array_t<uint8_t> arr) {
    PY_CHECK_DECODER();
    data_ref_arr_ = arr;
    auto data = data_ref_arr_.unchecked<1>();
    py::gil_scoped_release release;
    // code stream from one file
    PrepareStreamForOneImageDecoding(static_cast<const std::filesystem::path>(""), data.data(0), data.size());
}

CodeStream::CodeStream() {
}

CodeStream::~CodeStream() {
}

//
// CodeStream APIs
//

int CodeStream::width() {
    return m_width;
}

int CodeStream::height() {
    return m_height;
}