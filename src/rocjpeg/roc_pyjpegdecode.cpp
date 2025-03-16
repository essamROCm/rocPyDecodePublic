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
        .def("PyFreeHipDeviceMemory",&PyRocJpegUtils::PyFreeHipDeviceMemory)
        .def("test",&PyRocJpegUtils::test)
        ;
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

//
// TEST
//

int PyRocJpegUtils::test() {

    // simulate user input
    int argc = 5;
    char *argv[5] = {
    (char*)"test()\0",
    (char*)"-i\0",
    (char*)"/opt/rocm/share/rocjpeg/images/mug_420.jpg\0",
    (char*)"-o\0",
    (char*)"essam_test.yuv"};

    int device_id = 0;
    bool save_images = false;
    uint8_t num_components;
    uint32_t widths[ROCJPEG_MAX_COMPONENT] = {};
    uint32_t heights[ROCJPEG_MAX_COMPONENT] = {};
    uint32_t channel_sizes[ROCJPEG_MAX_COMPONENT] = {};
    uint32_t prior_channel_sizes[ROCJPEG_MAX_COMPONENT] = {};
    uint32_t num_channels = 0;
    int total_images = 0;
    double time_per_image_all = 0;
    double mpixels_all = 0;
    double images_per_sec = 0;
    std::string chroma_sub_sampling = "";
    std::string input_path, output_file_path;
    std::vector<std::string> file_paths = {};
    bool is_dir = false;
    bool is_file = false;
    RocJpegChromaSubsampling subsampling;
    RocJpegBackend rocjpeg_backend = ROCJPEG_BACKEND_HARDWARE;
    RocJpegHandle rocjpeg_handle = nullptr;
    RocJpegStreamHandle rocjpeg_stream_handle = nullptr;
    RocJpegImage output_image = {};
    RocJpegDecodeParams decode_params = {};
    RocJpegUtils rocjpeg_utils;
    uint64_t num_bad_jpegs = 0;
    uint64_t num_jpegs_with_411_subsampling = 0;
    uint64_t num_jpegs_with_unknown_subsampling = 0;
    uint64_t num_jpegs_with_unsupported_resolution = 0;

    RocJpegUtils::ParseCommandLine(input_path, output_file_path, save_images, device_id, rocjpeg_backend, decode_params, nullptr, nullptr, argc, (char **)argv);

    // print ParseCommandLine results
    std::cout << "================ Command Line Parsed Values ================" << std::endl;
    std::cout << "Input Path: " << input_path << std::endl;
    std::cout << "Output File Path: " << output_file_path << std::endl;
    std::cout << "Save Images: " << (save_images ? "true" : "false") << std::endl;
    std::cout << "Device ID: " << device_id << std::endl;
    // Print backend as integer (if no custom operator<< for RocJpegBackend)
    std::cout << "ROCJPEG Backend: " << static_cast<int>(rocjpeg_backend) << std::endl;
    // Print Decode Params (assuming public fields, adjust as needed)
    std::cout << "Decode Params:" << std::endl;
    std::cout << "  Output Format: " << static_cast<int>(decode_params.output_format) << std::endl;
    std::cout << "  Crop Rectangle: left=" << decode_params.crop_rectangle.left
            << ", top=" << decode_params.crop_rectangle.top
            << ", right=" << decode_params.crop_rectangle.right
            << ", bottom=" << decode_params.crop_rectangle.bottom << std::endl;
    std::cout << "  Target Dimension: width=" << decode_params.target_dimension.width
            << ", height=" << decode_params.target_dimension.height << std::endl;
    // Print argc and argv
    std::cout << "argc: " << argc << std::endl;
    std::cout << "argv: ";
    for (int i = 0; i < argc; ++i) {
        std::cout << argv[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "===========================================================" << std::endl;


    bool is_roi_valid = false;
    uint32_t roi_width;
    uint32_t roi_height;
    roi_width = decode_params.crop_rectangle.right - decode_params.crop_rectangle.left;
    roi_height = decode_params.crop_rectangle.bottom - decode_params.crop_rectangle.top;

    if (!RocJpegUtils::GetFilePaths(input_path, file_paths, is_dir, is_file)) {
        std::cerr << "ERROR: Failed to get input file paths!" << std::endl;
        return EXIT_FAILURE;
    }
    if (!RocJpegUtils::InitHipDevice(device_id)) {
        std::cerr << "ERROR: Failed to initialize HIP!" << std::endl;
        return EXIT_FAILURE;
    }

    CHECK_ROCJPEG(rocJpegCreate(rocjpeg_backend, device_id, &rocjpeg_handle));
    CHECK_ROCJPEG(rocJpegStreamCreate(&rocjpeg_stream_handle));

    std::vector<char> file_data;
    for (auto file_path : file_paths) {
        std::string base_file_name = file_path.substr(file_path.find_last_of("/\\") + 1);
        int image_count = 0;

        // Read an image from disk.
        std::ifstream input(file_path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
        if (!(input.is_open())) {
            std::cerr << "ERROR: Cannot open image: " << file_path << std::endl;
            return EXIT_FAILURE;
        }
        // Get the size
        std::streamsize file_size = input.tellg();
        input.seekg(0, std::ios::beg);
        // resize if buffer is too small
        if (file_data.size() < file_size) {
            file_data.resize(file_size);
        }
        if (!input.read(file_data.data(), file_size)) {
            std::cerr << "ERROR: Cannot read from file: " << file_path << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "Input file name: " << file_path << std::endl;
        RocJpegStatus rocjpeg_status = rocJpegStreamParse(reinterpret_cast<uint8_t*>(file_data.data()), file_size, rocjpeg_stream_handle);
        if (rocjpeg_status != ROCJPEG_STATUS_SUCCESS) {
            if (is_dir) {
                std::cout << std::endl;
                num_bad_jpegs++;
                continue;
            } else {
                std::cerr << "ERROR: Failed to parse the input jpeg stream with " << rocJpegGetErrorName(rocjpeg_status) << std::endl;
                return EXIT_FAILURE;
            }
        }

        CHECK_ROCJPEG(rocJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handle, &num_components, &subsampling, widths, heights));

        if (roi_width > 0 && roi_height > 0 && roi_width <= widths[0] && roi_height <= heights[0]) {
            is_roi_valid = true;
        }

        rocjpeg_utils.GetChromaSubsamplingStr(subsampling, chroma_sub_sampling);
        std::cout << "Input image resolution: " << widths[0] << "x" << heights[0] << std::endl;
        std::cout << "Chroma subsampling: " + chroma_sub_sampling  << std::endl;
        if (widths[0] < 64 || heights[0] < 64) {
            std::cerr << "The image resolution is not supported by VCN Hardware" << std::endl;
            if (is_dir) {
                num_jpegs_with_unsupported_resolution++;
                std::cout << std::endl;
                continue;
            } else
                return EXIT_FAILURE;
        }
        if (subsampling == ROCJPEG_CSS_411 || subsampling == ROCJPEG_CSS_UNKNOWN) {
            std::cerr << "The chroma sub-sampling is not supported by VCN Hardware" << std::endl;
            if (is_dir) {
                if (subsampling == ROCJPEG_CSS_411)
                    num_jpegs_with_411_subsampling++;
                if (subsampling == ROCJPEG_CSS_UNKNOWN)
                    num_jpegs_with_unknown_subsampling++;
                std::cout << std::endl;
                continue;
            } else
                return EXIT_FAILURE;
        }

        if (rocjpeg_utils.GetChannelPitchAndSizes(decode_params, subsampling, widths, heights, num_channels, output_image, channel_sizes)) {
            std::cerr << "ERROR: Failed to get the channel pitch and sizes" << std::endl;
            return EXIT_FAILURE;
        }

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

        if (is_roi_valid) {
            std::cout << "Cropped image resolution: " << roi_width << "x" <<  roi_height << std::endl;
        }
        std::cout << "Decoding started, please wait! ... " << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        CHECK_ROCJPEG(rocJpegDecode(rocjpeg_handle, rocjpeg_stream_handle, &decode_params, &output_image));
        auto end_time = std::chrono::high_resolution_clock::now();
        double time_per_image_in_milli_sec = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        double image_size_in_mpixels = (static_cast<double>(widths[0]) * static_cast<double>(heights[0]) / 1000000);
        image_count++;

        if (save_images) {
            std::string image_save_path = output_file_path;
            //if ROI is present, need to pass roi_width and roi_height
            uint32_t width = is_roi_valid ? roi_width : widths[0];
            uint32_t height = is_roi_valid ? roi_height : heights[0];
            if (is_dir) {
                rocjpeg_utils.GetOutputFileExt(decode_params.output_format, base_file_name, width, height, subsampling, image_save_path);
            }
            rocjpeg_utils.SaveImage(image_save_path, &output_image, width, height, subsampling, decode_params.output_format);
        }

        std::cout << "Average processing time per image (ms): " << time_per_image_in_milli_sec << std::endl;
        std::cout << "Average images per sec: " << 1000 / time_per_image_in_milli_sec << std::endl;

        if (is_dir) {
            std::cout << std::endl;
            total_images += image_count;
            time_per_image_all += time_per_image_in_milli_sec;
            mpixels_all += image_size_in_mpixels;
        }
        for (int i = 0; i < ROCJPEG_MAX_COMPONENT; i++) {
            prior_channel_sizes[i] = channel_sizes[i];
        }
    }

    for (int i = 0; i < num_channels; i++) {
        if (output_image.channel[i] != nullptr) {
            CHECK_HIP(hipFree((void *)output_image.channel[i]));
            output_image.channel[i] = nullptr;
        }
    }

    if (is_dir) {
        time_per_image_all = time_per_image_all / total_images;
        images_per_sec = 1000 / time_per_image_all;
        double mpixels_per_sec = mpixels_all * images_per_sec / total_images;
        std::cout << "Total decoded images: " << total_images << std::endl;
        if (num_bad_jpegs || num_jpegs_with_411_subsampling || num_jpegs_with_unknown_subsampling || num_jpegs_with_unsupported_resolution) {
            std::cout << "Total skipped images: " << num_bad_jpegs + num_jpegs_with_411_subsampling + num_jpegs_with_unknown_subsampling + num_jpegs_with_unsupported_resolution;
            if (num_bad_jpegs) {
                std::cout << " ,total images that cannot be parsed: " << num_bad_jpegs;
            }
            if (num_jpegs_with_411_subsampling) {
                std::cout << " ,total images with YUV 4:1:1 chroam subsampling: " << num_jpegs_with_411_subsampling;
            }
            if (num_jpegs_with_unknown_subsampling) {
                std::cout << " ,total images with unknwon chroam subsampling: " << num_jpegs_with_unknown_subsampling;
            }
            if (num_jpegs_with_unsupported_resolution) {
                std::cout << " ,total images with unsupported_resolution: " << num_jpegs_with_unsupported_resolution;
            }
            std::cout << std::endl;
        }
        if (total_images) {
            std::cout << "Average processing time per image (ms): " << time_per_image_all << std::endl;
            std::cout << "Average decoded images per sec (Images/Sec): " << images_per_sec << std::endl;
            std::cout << "Average decoded images size (Mpixels/Sec): " << mpixels_per_sec << std::endl;
        }
        std::cout << std::endl;
    }

    CHECK_ROCJPEG(rocJpegDestroy(rocjpeg_handle));
    CHECK_ROCJPEG(rocJpegStreamDestroy(rocjpeg_stream_handle));
    std::cout << "Decoding completed!" << std::endl;
    return EXIT_SUCCESS;
}