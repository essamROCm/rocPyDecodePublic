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
    //     py::class_<PyRocJpegDecoder> (m, "PyRocJpegDecoder")
    //     .def(py::init<int,int,rocDecVideoCodec,bool,const Rect *,int,int,uint32_t>(),
    //                 py::arg("device_id") = 0, py::arg("out_mem_type") = 0, py::arg("codec") = rocDecVideoCodec_HEVC, py::arg("force_zero_latency") = false, 
    //                 py::arg("p_crop_rect") = nullptr, py::arg("max_width") = 0, py::arg("max_height") = 0, py::arg("clk_rate") = 1000)
    //     //.def("GetDeviceinfo",&PyRocJpegDecoder::PyGetDeviceinfo)
    // ;
}
  

void PyRocJpegDecoder::InitConfigStructure() {
    // // init config struct
    // configInfo.reset(new ConfigInfo());
    // configInfo.get()->device_name = std::string("");
    // configInfo.get()->gcn_arch_name = std::string("");
    // configInfo.get()->pci_bus_id = 0;
    // configInfo.get()->pci_domain_id = 0;
    // configInfo.get()->pci_device_id = 0;
    // // init flush callback struct: support multi-resolution video streams
    // PyReconfigDumpFileStruct.b_dump_frames_to_file = false;
    // PyReconfigDumpFileStruct.output_file_name.clear();
    // PyReconfigParams.p_fn_reconfigure_flush = nullptr;
    // PyReconfigParams.p_reconfig_user_struct = nullptr;
    // PyReconfigParams.reconfig_flush_mode = 0;
}

PyRocJpegDecoder::~PyRocJpegDecoder() {
}
