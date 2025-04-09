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

#include "roc_pyjpeg_decoder.h"
#include "roc_pyjpeg_utils.h"
#include "roc_pyjpeg_codestream.h"

using namespace std;

void Decoder::exportToPython(py::module& m) {

    // PyJpegImages
    py::class_<PyJpegImages, shared_ptr<PyJpegImages>>(m, "PyJpegImages", py::module_local())
        .def(py::init<>())
        .def_readwrite("ext_buf", &PyJpegImages::ext_buf)
        .def("to_numpy_8bits", &PyJpegImages::to_numpy_8bits, py::arg("index") = 0, "Export a given plane (0=YUV default), or (0=Y, 1=U/UV, 2=V) as a NumPy uint 8 bits array")
        // DL Pack Tensor
        .def_property_readonly("shapeY", [](std::shared_ptr<PyJpegImages>& self) {
            return self->ext_buf[0]->shape();
            }, "Get the shape of the Y plane buffer as an array")
        .def_property_readonly("shapeUV", [](std::shared_ptr<PyJpegImages>& self) {
            return self->ext_buf[1]->shape();
            }, "Get the shape of the U plane buffer as an array")
        .def_property_readonly("shapeU", [](std::shared_ptr<PyJpegImages>& self) {
            return self->ext_buf[1]->shape();
            }, "Get the shape of the U plane buffer as an array")
        .def_property_readonly("shapeV", [](std::shared_ptr<PyJpegImages>& self) {
            return self->ext_buf[2]->shape();
            }, "Get the shape of the V plane buffer as an array")
        .def_property_readonly("shape", [](std::shared_ptr<PyJpegImages>& self) {
            return self->ext_buf[0]->shape();
            }, "Get the shape of the buffer as an array")
        .def_property_readonly("strides", [](std::shared_ptr<PyJpegImages>& self) {
                return self->ext_buf[0]->strides();
            }, "Get the strides of the buffer")
        .def_property_readonly("dtype", [](std::shared_ptr<PyJpegImages>& self) {
                return self->ext_buf[0]->dtype();
            }, "Get the data type of the buffer")
        .def("__dlpack__", [](std::shared_ptr<PyJpegImages>& self, py::object stream) {
            return self->ext_buf[0]->dlpack(stream);
            }, py::arg("stream") = NULL, "Export the buffer as a DLPack tensor")
        .def("__dlpack_device__", [](std::shared_ptr<PyJpegImages>& self) {
                return py::make_tuple(py::int_(static_cast<int>(DLDeviceType::kDLROCM)), py::int_(static_cast<int>(0)));
            }, "Get the device associated with the buffer");

    py::class_<Decoder>(m, "Decoder", "Decoder for image decoding operations. "
        "It provides methods to decode images from various sources such as files or data streams. ")
        .def(py::init(
            [](int device_id, int backend) {
                return new Decoder(device_id, backend);
            }),
            R"pbdoc(
            Initialize decoder.

            Args:
                device_id: Device id to execute decoding on.

                backend: CPU or GPU backend.

            )pbdoc",
             py::arg("device_id") = 0,  py::arg("backend") = 0)
        .def("read", py::overload_cast<DecodeSource*>(&Decoder::decode), R"pbdoc(
            Executes decoding from a filename.

            Args:
                path: File path to decode.

            Returns:
                rocPyJpeg.Image or None if the image cannot be decoded because of any reason.
        )pbdoc",
            "path"_a)
        .def("read", py::overload_cast<std::vector<DecodeSource*>&>(&Decoder::decode),
            R"pbdoc(
            Executes decoding from a batch of file paths.

            Args:
                path: List of file paths to decode.

            Returns:
                List of decoded rocPyJpeg.Image's. There is None in returned list on positions which could not be decoded.

            )pbdoc",
            "paths"_a)
        .def("decode", py::overload_cast<DecodeSource*>(&Decoder::decode), R"pbdoc(
            Executes decoding of data from a DecodeSource handle (code stream handle and an optional region of interest).

            Args:
                src: decode source object.

            Returns:
                rocPyJpeg.Image or None if the image cannot be decoded because of any reason.

            )pbdoc",
            "src"_a)
        .def("decode", py::overload_cast<std::vector<DecodeSource*>&>(&Decoder::decode),
            R"pbdoc(
            Executes decoding from a batch of DecodeSource handles (code stream handle and an optional region of interest).

            Args:
                srcs: List of DecodeSource objects

            Returns:
                List of decoded rocPyJpeg.Image's. There is None in returned list on positions which could not be decoded.
            )pbdoc",
            "srcs"_a);
    // clang-format on
}

Decoder::Decoder(int device_id, int backend) {
    m_device_id = device_id;
    m_backend = RocJpegBackend(backend);
    // init hip
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    if (!utils.InitHipDevice(m_device_id)) {
        std::cerr << "ERROR: Failed to initialize HIP!" << std::endl;
        status = ROCJPEG_STATUS_RUNTIME_ERROR;
    }
    // create decode obj
    PY_CHECK_ROCJPEG(rocJpegCreate(m_backend, m_device_id, &rocjpeg_handle));    
}

PyJpegImages Decoder::decode(DecodeSource* data) {
     // keep code stream ptr in class (alive)
    assert(data);
    reset_code_stream_store();
    reset_image_store();
    code_stream = *data->code_stream()->handle();

    // Here call to DECODE one single JPEG image
    RocJpegStatus status = rocJpegDecode(rocjpeg_handle, code_stream.stream_handle, &code_stream.decode_params, &code_stream.output_image);

    if (status != ROCJPEG_STATUS_SUCCESS) {
        std::cerr << "ERROR: Failed to decode image. Status code: " << status << std::endl;
        return image;
    }

    //
    // to export to python (use dlpack(GPU MEM) {and numpy host array})
    //
    if(code_stream.output_image.channel[0]) {
        uint32_t width = code_stream.width();
        uint32_t height = code_stream.height();
        uint32_t surf_stride = width;
        uint32_t bit_depth = 8;
        std::string type_str;
        std::vector<size_t> stride;
        // 8 bits
        type_str = static_cast<const char*>("|u1");
        stride.push_back(static_cast<size_t>(surf_stride));
        stride.push_back(sizeof(uint8_t));

        // use dlpack to export as GPU Tensor
        std::vector<size_t> shape{ static_cast<size_t>(height * 1.5f), static_cast<size_t>(width)};

        // dlpack - GPU MEM
        image.ext_buf[0]->LoadDLPack(shape, stride, bit_depth, type_str, (void *)code_stream.output_image.channel[0], m_device_id);

        // numpy - (CPU mem)
        AsNumpyHostTensor_8bits(&code_stream, &image);
    }
    return image; // return one image
}

std::vector<PyJpegImages> Decoder::decode(std::vector<DecodeSource*>& decode_source_arg) {

    std::vector<RocJpegStreamHandle> stream_handles;
    std::vector<RocJpegDecodeParams> decode_params_list;
    std::vector<RocJpegImage> destinations;
    int batch_size = decode_source_arg.size();

    if (!code_streams.empty()) {
        reset_images_store();
        reset_code_streams_store();
    }

    for (auto* data : decode_source_arg) {
        if (!data) {
            std::cerr << "Warning: Null DecodeSource* skipped." << std::endl;
            images.emplace_back();  // Push default/empty PyJpegImages
            continue;
        }

        // Add one by one
        code_streams.push_back(*data->code_stream()->handle());
        CodeStream& c_stream = code_streams.back();

        // prepare a list for 'rocJpegDecodeBatched()'
        stream_handles.push_back(c_stream.stream_handle);
        decode_params_list.push_back(c_stream.decode_params);
        destinations.push_back(c_stream.output_image);
    }

    // Here call to DECODE ALL in the batch one time
    RocJpegStatus status = rocJpegDecodeBatched(
                                    rocjpeg_handle,
                                    stream_handles.data(),
                                    batch_size,
                                    decode_params_list.data(),
                                    destinations.data()
                                    );

    if (status != ROCJPEG_STATUS_SUCCESS) {
        std::cerr << "ERROR: Failed to decode the images batch . Status code: " << status << std::endl;
        return images;
    }
    
    //
    // to export to python (use dlpack(GPU MEM) {and numpy host array})
    //
    for(int i=0; i<batch_size; i++) {
        
        PyJpegImages img;
        images.push_back(img);

        if(code_streams[i].output_image.channel[0]) {
        
            uint32_t width = code_streams[i].width();
            uint32_t height = code_streams[i].height();
            uint32_t surf_stride = width;
            uint32_t bit_depth = 8;
            std::string type_str;
            std::vector<size_t> stride;
            // 8 bits
            type_str = static_cast<const char*>("|u1");
            stride.push_back(static_cast<size_t>(surf_stride));
            stride.push_back(sizeof(uint8_t));

            // use dlpack to export as GPU Tensor
            std::vector<size_t> shape{ static_cast<size_t>(height * 1.5f), static_cast<size_t>(width)};

            // dlpack - GPU MEM
            images[i].ext_buf[0]->LoadDLPack(shape, stride, bit_depth, type_str, (void *)code_streams[i].output_image.channel[0], m_device_id);

            // numpy - (CPU mem)
            AsNumpyHostTensor_8bits(&code_streams[i], &images[i]);
        }
    }
    return images; // return the images list
}

void Decoder::AsNumpyHostTensor_8bits(CodeStream* code_stream, PyJpegImages* image) {
    // use current stream info
    RocJpegImage *output_image = &code_stream->output_image;
    uint32_t img_width = code_stream->width();
    uint32_t img_height = code_stream->height();
    RocJpegChromaSubsampling subsampling = code_stream->subsampling;
    RocJpegOutputFormat output_format = code_stream->decode_params.output_format;
    hipError_t hip_status = hipSuccess;
    uint32_t widths[ROCJPEG_MAX_COMPONENT] = {}; // not used for now
    uint32_t heights[ROCJPEG_MAX_COMPONENT] = {};

    switch (output_format) {
        case ROCJPEG_OUTPUT_NATIVE:
            switch (subsampling) {
                case ROCJPEG_CSS_444:
                    widths[2] = widths[1] = widths[0] = img_width;
                    heights[2] = heights[1] = heights[0] = img_height;
                    break;
                case ROCJPEG_CSS_440:
                    widths[2] = widths[1] = widths[0] = img_width;
                    heights[0] = img_height;
                    heights[2] = heights[1] = img_height >> 1;
                    break;
                case ROCJPEG_CSS_422:
                    widths[0] = img_width * 2;
                    heights[0] = img_height;
                    break;
                case ROCJPEG_CSS_420:
                    widths[1] = widths[0] = img_width;
                    heights[0] = img_height;
                    heights[1] = img_height >> 1;
                    break;
                case ROCJPEG_CSS_400:
                    widths[0] = img_width;
                    heights[0] = img_height;
                    break;
                default:
                    std::cout << "Unknown chroma subsampling!" << std::endl;
                    return;
            }
            break;
        case ROCJPEG_OUTPUT_YUV_PLANAR:
            switch (subsampling) {
                case ROCJPEG_CSS_444:
                    widths[2] = widths[1] = widths[0] = img_width;
                    heights[2] = heights[1] = heights[0] = img_height;
                    break;
                case ROCJPEG_CSS_440:
                    widths[2] = widths[1] = widths[0] = img_width;
                    heights[0] = img_height;
                    heights[2] = heights[1] = img_height >> 1;
                    break;
                case ROCJPEG_CSS_422:
                    widths[0] = img_width;
                    widths[2] = widths[1] = widths[0] >> 1;
                    heights[2] = heights[1] = heights[0] = img_height;
                    break;
                case ROCJPEG_CSS_420:
                    widths[0] = img_width;
                    widths[2] = widths[1] = widths[0] >> 1;
                    heights[0] = img_height;
                    heights[2] = heights[1] = img_height >> 1;
                    break;
                case ROCJPEG_CSS_400:
                    widths[0] = img_width;
                    heights[0] = img_height;
                    break;
                default:
                    std::cout << "Unknown chroma subsampling!" << std::endl;
                    return;
            }
            break;
        case ROCJPEG_OUTPUT_Y:
            widths[0] = img_width;
            heights[0] = img_height;
            break;
        case ROCJPEG_OUTPUT_RGB:
            widths[0] = img_width * 3;
            heights[0] = img_height;
            break;
        case ROCJPEG_OUTPUT_RGB_PLANAR:
            widths[2] = widths[1] = widths[0] = img_width;
            heights[2] = heights[1] = heights[0] = img_height;
            break;
        default:
            std::cout << "Unknown output format!" << std::endl;
            return;
    }

    uint32_t channel0_size = output_image->pitch[0] * heights[0];
    uint32_t channel1_size = output_image->pitch[1] * heights[1];
    uint32_t channel2_size = output_image->pitch[2] * heights[2];
    uint32_t output_image_size = channel0_size + channel1_size + channel2_size;

    if (image->cpu_data_temp_8bits == nullptr) {
        image->cpu_data_temp_8bits = new uint8_t[output_image_size];
    }

    // copy channel0
    if (channel1_size != 0 && output_image->channel[0] != nullptr) {
        PY_CHECK_HIP(hipMemcpyDtoH((void *)image->cpu_data_temp_8bits, output_image->channel[0], channel0_size));
    }
    // copy channel1
    if (channel1_size != 0 && output_image->channel[1] != nullptr) {
        uint8_t *channel1_hst_ptr = image->cpu_data_temp_8bits + channel0_size;
        PY_CHECK_HIP(hipMemcpyDtoH((void *)channel1_hst_ptr, output_image->channel[1], channel1_size));
    }
    // copy channel2
    if (channel2_size != 0 && output_image->channel[2] != nullptr) {
        uint8_t *channel2_hst_ptr = image->cpu_data_temp_8bits + channel0_size + channel1_size;
        PY_CHECK_HIP(hipMemcpyDtoH((void *)channel2_hst_ptr, output_image->channel[2], channel2_size));
    }
}

void Decoder::reset_code_stream_store() {
    // Check if single code_stream has any entries
    if (code_stream.stream_handle != nullptr) {
        std::cout << "Cleaning up single image code_stream." << std::endl;
        // Destroy any allocated MEM
        for (int i = 0; i < code_stream.num_channels; i++) {
            if (code_stream.output_image.channel[i] != nullptr) {
                PY_CHECK_HIP(hipFree((void *)code_stream.output_image.channel[i]));
                code_stream.output_image.channel[i] = nullptr;
            }
        }
        // Destroy stream
        PY_CHECK_ROCJPEG(rocJpegStreamDestroy(code_stream.stream_handle));
    }
}

void Decoder::reset_code_streams_store() {
    if (!code_streams.empty()) {
        std::cout << "Cleaning up batch images code_streams (" << code_streams.size() << " instances.)" << std::endl;
        for (auto& cs : code_streams) {
            // Destroy any allocated MEM
            for (int i = 0; i < cs.num_channels; i++) {
                if (cs.output_image.channel[i] != nullptr) {
                    PY_CHECK_HIP(hipFree((void *)cs.output_image.channel[i]));
                    cs.output_image.channel[i] = nullptr;
                }
            }
            // Destroy stream
            if(cs.stream_handle) {
                PY_CHECK_ROCJPEG(rocJpegStreamDestroy(cs.stream_handle));
            }
        }
        // clear the vector after cleanup
        code_streams.clear();
        code_streams.shrink_to_fit();
    }
}

void Decoder::reset_image_store() {
    std::cout << "Cleaning up single image." << std::endl;
    // Free host temp memory if allocated
    if (image.cpu_data_temp_8bits) {
        free(image.cpu_data_temp_8bits);
        image.cpu_data_temp_8bits = nullptr;
    }
    // Re-initialize shared GPU buffer wrappers
    for (auto& buf : image.ext_buf) {
        buf = std::make_shared<BufferInterface>();
    }
}

void Decoder::reset_images_store() {
    if (!images.empty()) {
        std::cout << "Cleaning up batch images (" << images.size() << " instances.)" << std::endl;
        for (auto& img : images) {
            if (img.cpu_data_temp_8bits) {
                free(img.cpu_data_temp_8bits);
                img.cpu_data_temp_8bits = nullptr;
            }
            for (auto& buf : img.ext_buf) {
                buf = std::make_shared<BufferInterface>();
            }
        }
        // clear the vector after cleanup
        images.clear();
        images.shrink_to_fit();
    }
}

Decoder::~Decoder() {
    // de-alloc, clean all about code_stream
    reset_code_stream_store();
    reset_code_streams_store();
    reset_image_store();
    reset_images_store();
    // delete/destroy MAIN decode obj
    if(rocjpeg_handle) {
        PY_CHECK_ROCJPEG(rocJpegDestroy(rocjpeg_handle));
        rocjpeg_handle = nullptr;
    }
}
