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

#include "common/roc_pybuffer.h"
#include "roc_pyjpeg_test.h"
#include "common/roc_pydlpack.h"

#include <iostream>
#include <vector>
#include <memory>
#include <cstring> // For memset

using namespace std;

void PyJpegTest::ExportToPython(py::module &m) {
    py::class_<PyJpegTest>(m, "PyJpegTest")
        .def(py::init<>())
        .def("test_all", &PyJpegTest::test_all, "Test all available functionality.")       
    ;
}

void PyJpegTest::test_all() {
    test_roc_pybuffer();
}

void PyJpegTest::test_roc_pybuffer() {  
    // ------------- Setup mock data -------------
    size_t ndim = 2;
    vector<size_t> shape = {4, 4};     // 4x4 tensor
    vector<size_t> stride = {4 * sizeof(uint8_t), sizeof(uint8_t)};
    uint32_t bit_depth = 8;
    string type_str = "|u1";
    int device_id = 0;
    uint8_t dummy_data[16];
    memset(dummy_data, 1, sizeof(dummy_data));

    // 1. Create a dummy DLManagedTensor
    DLManagedTensor managedTensor = {};
    managedTensor.dl_tensor.data = dummy_data;
    managedTensor.dl_tensor.device.device_type = kDLROCM;
    managedTensor.dl_tensor.device.device_id = device_id;
    managedTensor.dl_tensor.ndim = ndim;
    managedTensor.dl_tensor.shape = new int64_t[ndim]{4, 4};
    managedTensor.dl_tensor.strides = new int64_t[ndim]{4, 1};
    managedTensor.dl_tensor.byte_offset = 0;
    managedTensor.dl_tensor.dtype.code = kDLUInt;
    managedTensor.dl_tensor.dtype.bits = 8;
    managedTensor.dl_tensor.dtype.lanes = 1;

    // 2. Construct a DLPackPyTensor using managedTensor
    DLPackPyTensor dlTensorShared(std::move(managedTensor));

    // 3. Now construct the BufferInterface
    auto buffer = std::make_shared<BufferInterface>(std::move(dlTensorShared));

    // ------------- Test all methods -------------
    
    // shape()
    auto shape_out = buffer->shape();
    cout << "Shape: ";
    for (auto item : shape_out) cout << item.cast<int>() << " ";
    cout << endl;

    // strides()
    auto strides_out = buffer->strides();
    cout << "Strides: ";
    for (auto item : strides_out) cout << item.cast<int>() << " ";
    cout << endl;

    // dtype()
    auto dtype_out = buffer->dtype();
    cout << "Dtype: " << dtype_out << endl;

    // data()
    void* data_ptr = buffer->data();
    cout << "Data pointer: " << data_ptr << endl;

    // dlpack()
    py::object dummy_stream = py::int_(1);  // dummy stream value
    auto capsule = buffer->dlpack(dummy_stream);
    cout << "DLPack capsule created successfully." << endl;

    // dlTensor()
    const DLTensor& tensor_ref = buffer->dlTensor();
    cout << "DLTensor ndim: " << tensor_ref.ndim << endl;

    // ------------- LoadDLPack test -------------
    BufferInterface new_buffer;
    int ret = new_buffer.LoadDLPack(shape, stride, bit_depth, type_str, dummy_data, device_id);
    cout << "LoadDLPack returned: " << ret << endl;

    // ------------- Clean up shape and strides manually -------------
    delete[] managedTensor.dl_tensor.shape;
    delete[] managedTensor.dl_tensor.strides;
}
    