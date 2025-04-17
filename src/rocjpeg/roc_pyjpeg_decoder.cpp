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
            [](int device_id, int backend, RocJpegOutputFormat output_format = ROCJPEG_OUTPUT_RGB) {
                return new Decoder(device_id, backend, output_format);
            }),
            R"pbdoc(
            Initialize decoder.

            Args:
                device_id: Device id to execute decoding on.

                backend: CPU or GPU backend.

            )pbdoc",
             py::arg("device_id") = 0,  py::arg("backend") = 0, py::arg("output_format") = ROCJPEG_OUTPUT_RGB)
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
            Executes decoding of data from a DecodeSource handle (code stream handle).

            Args:
                src: decode source object.

            Returns:
                rocPyJpeg.Image or None if the image cannot be decoded because of any reason.

            )pbdoc",
            "src"_a)
        .def("decode", py::overload_cast<std::vector<DecodeSource*>&>(&Decoder::decode),
            R"pbdoc(
            Executes decoding from a batch of DecodeSource handles (code stream handle).

            Args:
                srcs: List of DecodeSource objects

            Returns:
                List of decoded rocPyJpeg.Image's. There is None in returned list on positions which could not be decoded.
            )pbdoc",
            "srcs"_a);
    // clang-format on
}

Decoder::Decoder(int device_id, int backend, RocJpegOutputFormat output_format) {
    set_output_format(output_format);   // save user choice if sent for output format
    m_device_id = device_id;            // save user device id
    m_backend = RocJpegBackend(backend);// save user backend choise
    // init hip
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    PyRocJpegUtils rocjpeg_utils;
    if (!rocjpeg_utils.InitHipDevice(m_device_id)) {
        std::cerr << "ERROR: Failed to initialize HIP!" << std::endl;
        status = ROCJPEG_STATUS_RUNTIME_ERROR;
        rocjpeg_handle = nullptr;
        return;
    }
    // create decode obj
    PY_CHECK_ROCJPEG(rocJpegCreate(m_backend, m_device_id, &rocjpeg_handle));
}
PyJpegImages Decoder::decode(DecodeSource* data) {
    // keep code stream ptr in class (alive)
    assert(data);
    reset_code_stream_store();
    reset_image_store();

    // to return at least an empty/invalid one image record
    PyJpegImages img;
    img.set_valid(false);
    images_.push_back(img);  // Push default/empty/invalid PyJpegImages

    // get current data/file associated code_stream instance
    code_stream.push_back(*data->code_stream()->handle());

    // runtime sanity check
    if(!code_stream.back().is_valid()) {
        return images_.back(); // last item is this instance
    }

    // Here call to DECODE one single JPEG image
    RocJpegStatus status = rocJpegDecode(rocjpeg_handle, code_stream.back().stream_handle, &code_stream.back().decode_params, &code_stream.back().output_image);
    if (status != ROCJPEG_STATUS_SUCCESS) {
        std::cerr << "ERROR: Failed to decode image. Status code: " << status << std::endl;
        return images_.back();
    }

    // to export to python (use dlpack(GPU MEM) {and numpy host array}) -- GPU Tensor
    to_dlpack_tensor(&code_stream.back(), &images_.back());
    images_.back().set_valid(true);
    return images_.back(); // return one image
}

std::vector<PyJpegImages> Decoder::decode(std::vector<DecodeSource*>& decode_source_arg) {
    int count_of_valid_instances = 0;
    std::vector<RocJpegStreamHandle> stream_handles;
    std::vector<RocJpegDecodeParams> decode_params_list;
    std::vector<RocJpegImage> destinations;

    reset_code_stream_store();
    reset_image_store();

    int batch_size = decode_source_arg.size();

    // we return a list of images anyway, create empty count of batch_size
    PyJpegImages img;
    img.set_valid(false);
    for (int x = 0; x <batch_size; ++x) {
        images_.push_back(img);
    }

    // loop the whole list length
    for (auto* data : decode_source_arg) {
        if (!data) {
            std::cerr << "Warning: Null DecodeSource* skipped." << std::endl;
            // push empty/invalid record
            CodeStream empty_code_stream;
            empty_code_stream.set_valid(false);
            code_stream.push_back(empty_code_stream);
            continue;
        }

        // Add one by one
        code_stream.push_back(*data->code_stream()->handle());
        CodeStream& c_stream = code_stream.back();

        // only the valid instances
        if(c_stream.is_valid() && (c_stream.stream_handle!=nullptr)) {
            // prepare a list for 'rocJpegDecodeBatched()'
            stream_handles.push_back(c_stream.stream_handle);
            decode_params_list.push_back(c_stream.decode_params);
            destinations.push_back(c_stream.output_image);
            count_of_valid_instances++;
        }
    }

    // at least one
    RocJpegStatus status = ROCJPEG_STATUS_SUCCESS;
    if(count_of_valid_instances > 0) {
        // Here call to DECODE ALL in the batch one time
        status = rocJpegDecodeBatched(  rocjpeg_handle,
                                        stream_handles.data(),
                                        count_of_valid_instances, // <= the batch_size
                                        decode_params_list.data(),
                                        destinations.data()
                                        );
        if (status != ROCJPEG_STATUS_SUCCESS) {
            std::cerr << "ERROR: Failed to decode the image batch . Status code: " << status << std::endl;
            return images_; // return the image list
        }
    }

    // to export to python (use dlpack(GPU MEM) {and numpy host array})
    if (status == ROCJPEG_STATUS_SUCCESS) {
        for(int i=0; i<batch_size; i++) {
            // is it an OK instance?
            images_[i].set_valid(false);
            if(!code_stream[i].is_valid()) {
                continue;
            }
            // GPU Tensor
            to_dlpack_tensor(&code_stream[i], &images_[i]);
            // mark it as OK image to use
            images_[i].set_valid(true);
        }
    }
    return images_; // return the image list
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
    switch(output_format) {
        case ROCJPEG_OUTPUT_RGB_PLANAR: { // each color plane in a channel separately R[0], G[1], and B[2]
            uint32_t surf_stride[3] = {widths[0], widths[1], widths[2]}; // ROCJPEG_OUTPUT_RGB_PLANAR all same width = img_width
            for(int i=0; i<3; i++) {
                std::vector<size_t> shape{ static_cast<size_t>(heights[i]), static_cast<size_t>(widths[i])}; // depend on get_widths_heights_from_output_format()
                std::vector<size_t> stride{ static_cast<size_t>(surf_stride[i]), 1, 0};
                // RGB PLANAR using VCN JPEG decoder @ first, second, and third channel of RocJpegImage
                image->ext_buf[i]->LoadDLPack(shape, stride, bit_depth, type_str, (void *)code_stream->output_image.channel[i], m_device_id); // m_device_id was set at the constructor
            }
        }
        break;
        default:
        case ROCJPEG_OUTPUT_RGB: { // all the image RGB interleaved in one channel [0]
            uint32_t surf_stride = widths[0]; // ROCJPEG_OUTPUT_RGB width is * 3 for RGB interleaved
            std::vector<size_t> shape{ static_cast<size_t>(heights[0]), static_cast<size_t>(widths[0]/3), 3}; // widths[0]/3 for ROCJPEG_OUTPUT_RGB
            std::vector<size_t> stride{ static_cast<size_t>(surf_stride), 1, 0}; // python assumes same dim for both shape & strides
            // interleaved RGB using VCN JPEG decoder written to first channel of RocJpegImage
            image->ext_buf[0]->LoadDLPack(shape, stride, bit_depth, type_str, (void *)code_stream->output_image.channel[0], m_device_id); // m_device_id was set at the constructor
        }
        break;
    }
    return true;
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
                PY_CHECK_ROCJPEG(rocJpegStreamDestroy(cs.stream_handle));
                cs.stream_handle = nullptr;
            }
        }
        // clear the vector after cleanup
        code_stream.clear();
    }
}

void Decoder::reset_image_store() {
    if (!images_.empty()) {
        // std::cout << "Cleaning up batch image store (" << image.size() << " instances.)" << std::endl;
        for (auto& img : images_) {
            if(img.is_valid()) {
                for (auto& buf : img.ext_buf) {
                    buf = std::make_shared<BufferInterface>();
                }
            }
        }
        // clear the vector after cleanup
        images_.clear();
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
    if(rocjpeg_handle != nullptr) {
        PY_CHECK_ROCJPEG(rocJpegDestroy(rocjpeg_handle));
        rocjpeg_handle = nullptr;
    }
}
