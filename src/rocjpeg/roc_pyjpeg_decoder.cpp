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
        .def("to_numpy", &PyJpegImages::to_numpy, py::arg("index") = 0, "Export a given plane (0=YUV default), or (0=Y, 1=U/UV, 2=V) as a NumPy uint 8 bits array")
        .def("is_valid",&PyJpegImages::is_valid, "Indicates if the image is OK to use.")
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
        .def("set_output_format",&Decoder::set_output_format)
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

Decoder::Decoder(int device_id, int backend, RocJpegOutputFormat output_format) {
    set_output_format(output_format);   // save user choise if sent for output format
    m_device_id = device_id;            // save user device id
    m_backend = RocJpegBackend(backend);// save user backend choise
    // init hip
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    PyRocJpegUtils rocjpeg_utils;
    if (!rocjpeg_utils.InitHipDevice(m_device_id)) {
        std::cerr << "ERROR: Failed to initialize HIP!" << std::endl;
        status = ROCJPEG_STATUS_RUNTIME_ERROR;
    }
    // create decode obj
    PY_CHECK_ROCJPEG(rocJpegCreate(m_backend, m_device_id, &rocjpeg_handle));

    std::cout << "USER output_format: " << static_cast<int>(user_output_format) << std::endl;
    std::cout << "m_device_id: " << static_cast<int>(m_device_id) << std::endl;
    std::cout << "m_backend :" << static_cast<int>(m_backend) << std::endl;
}

PyJpegImages Decoder::decode(DecodeSource* data) {
    // keep code stream ptr in class (alive)
    assert(data);

    reset_code_stream_store();
    reset_image_store();

    // to return at least an empty/invalid one image record
    PyJpegImages img;
    img.set_valid(false);
    image.push_back(img);  // Push default/empty/invalid PyJpegImages

    // get current data/file associated code_stream instance
    code_stream.push_back(*data->code_stream()->handle());

    // runtime sanity check
    if(!code_stream.back().is_valid()) {
        return image.back(); // last item is this instance
    }

    // Here call to DECODE one single JPEG image
    RocJpegStatus status = rocJpegDecode(rocjpeg_handle, code_stream.back().stream_handle, &code_stream.back().decode_params, &code_stream.back().output_image);

    if (status != ROCJPEG_STATUS_SUCCESS) {
        std::cerr << "ERROR: Failed to decode image. Status code: " << status << std::endl;
        return image.back();
    }

    // to export to python (use dlpack(GPU MEM) {and numpy host array})

    // GPU Tensor
    to_dlpack_tensor(&code_stream.back(), &image.back());
    // numpy - (CPU mem)
    if(to_numpy_tensor(&code_stream.back(), &image.back()) == true)
        image.back().set_valid(true);// mark it as valid

    return image.back(); // return one image
}

std::vector<std::shared_ptr<PyJpegImages>> Decoder::decode(std::vector<DecodeSource*>& decode_source_arg) {
    int count_of_valid_instances = 0;
    std::vector<RocJpegStreamHandle> stream_handles;
    std::vector<RocJpegDecodeParams> decode_params_list;
    std::vector<RocJpegImage> destinations;

    reset_code_stream_store();
    reset_image_store();

    int batch_size = decode_source_arg.size();

    // save current index at the main store
    int ndx = code_stream.size();
    int start_index = (ndx<=0) ? 0 : ndx;

    // we return a list of images anyway, create empty count of batch_size
    PyJpegImages img;
    img.set_valid(false);
    for (int x = start_index; x < start_index + batch_size; ++x) {
        image.push_back(img);
    }

    // loop the whole list length
    for (auto* data : decode_source_arg) {
        if (!data) {
            std::cerr << "Warning: Null DecodeSource* skipped." << std::endl;
            // push empty/invalid record
            CodeStream empty_code_stream;
            empty_code_stream.set_valid(false);
            code_stream.push_back(empty_code_stream);
            std::cout << "Added empty code stream" << std::endl;
            continue;
        }

        // Add one by one
        code_stream.push_back(*data->code_stream()->handle());
        CodeStream& c_stream = code_stream.back();

        // only the valid instances
        if(c_stream.is_valid()) {
            // prepare a list for 'rocJpegDecodeBatched()'
            stream_handles.push_back(c_stream.stream_handle);
            decode_params_list.push_back(c_stream.decode_params);
            destinations.push_back(c_stream.output_image);
            count_of_valid_instances++;
        }
    }

    // at least one
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    if(count_of_valid_instances) {
        // Here call to DECODE ALL in the batch one time
        status = rocJpegDecodeBatched(  rocjpeg_handle,
                                        stream_handles.data(),
                                        count_of_valid_instances, // <= the batch_size
                                        decode_params_list.data(),
                                        destinations.data()
                                        );
        if (status != ROCJPEG_STATUS_SUCCESS) {
            std::cerr << "ERROR: Failed to decode the image batch . Status code: " << status << std::endl;
        }
    }

    // to export to python (use dlpack(GPU MEM) {and numpy host array})

    if (status == ROCJPEG_STATUS_SUCCESS) {
        for(int i=start_index; i<(start_index+batch_size); i++) {
            // is it an OK instance?
            image[i].set_valid(false);
            if(!code_stream[i].is_valid()) {
                continue;
            }
            // GPU Tensor
            to_dlpack_tensor(&code_stream[i], &image[i]);
            // numpy - (CPU mem)
            if(to_numpy_tensor(&code_stream[i], &image[i]) == false) // [i] is the index of the 'OK' image record
                continue;
            // mark it as OK
            image[i].set_valid(true);
        }
    }

    image_list.clear();  // Ensure the output list is empty
    size_t end_index = std::min( static_cast<size_t>(start_index + batch_size), static_cast<size_t>(image.size()));
    for (size_t i = start_index; i < end_index; ++i) {
        image_list.push_back(std::make_shared<PyJpegImages>(image[i]));
    }
    return image_list; // return the image list
}

bool Decoder::get_widths_heights_from_output_format(std::vector<uint32_t>& widths, std::vector<uint32_t>& heights, uint32_t img_width, uint32_t img_height, RocJpegOutputFormat output_format, RocJpegChromaSubsampling subsampling) {
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
                    return false;
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
                    return false;
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
            return false;
    }
    return true;
}

bool Decoder::to_dlpack_tensor(CodeStream* code_stream, PyJpegImages* image) {
    uint32_t img_width = code_stream->width();
    uint32_t img_height = code_stream->height();
    RocJpegChromaSubsampling subsampling = code_stream->subsampling;
    RocJpegOutputFormat output_format = code_stream->decode_params.output_format;
    std::vector<uint32_t> widths;
    std::vector<uint32_t> heights;
    widths.resize(ROCJPEG_MAX_COMPONENT);
    heights.resize(ROCJPEG_MAX_COMPONENT);
     if( get_widths_heights_from_output_format(widths, heights, img_width, img_height, output_format, subsampling) == false)
        return false;
    // 8 bits - Assuming output_format = ROCJPEG_OUTPUT_RGB (interleaved RGB)
    uint32_t bit_depth = 8;
    std::string type_str(static_cast<const char*>("|u1"));
    uint32_t surf_stride = widths[0]; // ROCJPEG_OUTPUT_RGB width is * 3 for RGB interleaved
    std::vector<size_t> shape{ static_cast<size_t>(heights[0]), static_cast<size_t>(widths[0]/3), 3};
    std::vector<size_t> stride{ static_cast<size_t>(surf_stride), 1, 0}; // python assumes same dim for both shape & strides
    // interleaved RGB using VCN JPEG decoder written to first channel of RocJpegImage
    image->ext_buf[0]->LoadDLPack(shape, stride, bit_depth, type_str, (void *)code_stream->output_image.channel[0], m_device_id);
    return true;
}

bool Decoder::to_numpy_tensor(CodeStream* code_stream, PyJpegImages* image) {
    // use current stream info
    RocJpegImage *output_image = &code_stream->output_image;
    uint32_t img_width = code_stream->width();
    uint32_t img_height = code_stream->height();
    RocJpegChromaSubsampling subsampling = code_stream->subsampling;
    RocJpegOutputFormat output_format = code_stream->decode_params.output_format;
    hipError_t hip_status = hipSuccess;
    std::vector<uint32_t> widths;
    std::vector<uint32_t> heights;

    widths.resize(ROCJPEG_MAX_COMPONENT);
    heights.resize(ROCJPEG_MAX_COMPONENT);

    if( get_widths_heights_from_output_format(widths, heights, img_width, img_height, output_format, subsampling) == false)
        return false;

    uint32_t channel0_size = output_image->pitch[0] * heights[0];
    uint32_t channel1_size = output_image->pitch[1] * heights[1];
    uint32_t channel2_size = output_image->pitch[2] * heights[2];
    uint32_t output_image_size = channel0_size + channel1_size + channel2_size;

    if (image->cpu_data_temp_8bits == nullptr) {
        image->cpu_data_temp_8bits = new uint8_t[output_image_size];
    }

    // copy channel0 (interleaved RGB)
    if (channel0_size != 0 && output_image->channel[0] != nullptr) {
        hip_status = hipMemcpyDtoH((void *)image->cpu_data_temp_8bits, output_image->channel[0], channel0_size);
        if (hip_status != hipSuccess)
            return false;
    }
    // copy channel1
    if (channel1_size != 0 && output_image->channel[1] != nullptr) {
        uint8_t *channel1_hst_ptr = image->cpu_data_temp_8bits + channel0_size;
        hip_status = hipMemcpyDtoH((void *)channel1_hst_ptr, output_image->channel[1], channel1_size);
        if (hip_status != hipSuccess)
            return false;
    }
    // copy channel2
    if (channel2_size != 0 && output_image->channel[2] != nullptr) {
        uint8_t *channel2_hst_ptr = image->cpu_data_temp_8bits + channel0_size + channel1_size;
        hip_status = hipMemcpyDtoH((void *)channel2_hst_ptr, output_image->channel[2], channel2_size);
        if (hip_status != hipSuccess)
        return false;
    }
    return true;
}

void Decoder::reset_code_stream_store() {
    if (!code_stream.empty()) {
        // std::cout << "Cleaning up batch image code_stream store (" << code_stream.size() << " instances.)" << std::endl;
        for (auto& cs : code_stream) {
            if (cs.stream_handle != nullptr && cs.is_valid()) {
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
        }
        // clear the vector after cleanup
        code_stream.clear();
        code_stream.shrink_to_fit();
    }
}

void Decoder::reset_image_store() {
    if (!image.empty()) {
        // std::cout << "Cleaning up batch image store (" << image.size() << " instances.)" << std::endl;
        for (auto& img : image) {
            if(img.is_valid()) {
                if (img.cpu_data_temp_8bits) {
                    free(img.cpu_data_temp_8bits);
                    img.cpu_data_temp_8bits = nullptr;
                }
                for (auto& buf : img.ext_buf) {
                    buf = std::make_shared<BufferInterface>();
                }
            }
        }
        // clear the vector after cleanup
        image.clear();
        image.shrink_to_fit();
    }
}

// set the user desired output_format
void Decoder::set_output_format(RocJpegOutputFormat output_format) {
    if(output_format != ROCJPEG_OUTPUT_RGB && output_format != ROCJPEG_OUTPUT_RGB_PLANAR) {
        std::cerr << "ERROR: Unspported output format, defaulting to ROCJPEG_OUTPUT_RGB." << std::endl;
        user_output_format = ROCJPEG_OUTPUT_RGB; // default
        return;
    }
    user_output_format = output_format;
}

Decoder::~Decoder() {
    // de-alloc, clean all about code_stream
    reset_code_stream_store();
    reset_image_store();

    // delete/destroy MAIN decode obj
    if(rocjpeg_handle) {
        PY_CHECK_ROCJPEG(rocJpegDestroy(rocjpeg_handle));
        rocjpeg_handle = nullptr;
    }
}
