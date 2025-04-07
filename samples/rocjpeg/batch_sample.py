# Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import torch
import pyRocJpegDecode.decoder as jdec

# example of BATCHED images decode
device_id = 0
backend = 0

image_paths = ["/opt/rocm/share/rocjpeg/images/mug_400.jpg", "/opt/rocm/share/rocjpeg/images/mug_420.jpg", "/opt/rocm/share/rocjpeg/images/mug_422.jpg"]
batch_size = len(image_paths)

decoder = jdec.decoder(device_id, backend)

images = decoder.decode(image_paths)

# printout all info
for i in range(batch_size):

    # print GPU tensor details
    print("\n", type(images[i]))
    print("Tensor Shape:   ", images[i].ext_buf[0].shape)
    print("Tensor Strides: ", images[i].ext_buf[0].strides)
    print("Tensor dType:   ", images[i].ext_buf[0].dtype)
    print("Tensor Device:  ", images[i].ext_buf[0].__dlpack_device__(), "\n")

    # ------------
    # GPU Tensor
    # ------------
    yuv_tensor = torch.from_dlpack(images[i].ext_buf[0].__dlpack__(images[i]))

    print(f"Device:\t\t\t {yuv_tensor.device}")
    print(f"Tensor GPU MEM address:\t {hex(yuv_tensor.data_ptr())}\n")

    # ------------
    #  CPU Tensor
    # ------------
    tensor = torch.from_numpy(images[i].to_numpy())

    print(f"Tensor tensor: {tensor.device}")  # Should output: cpu
    print(f"Tensor CPU MEM address:\t {hex(tensor.data_ptr())}\n")
    print("Shape:", tensor.shape)
    print("Dtype:", tensor.dtype)

    # Print top-left 5x5 region (for 2D tensors)
    print(tensor[:5, :5], "\n")
