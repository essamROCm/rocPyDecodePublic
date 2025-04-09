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
import numpy as np

def printout_tensor_info(image):
    # print GPU tensor details
    print("\n", type(image))
    print("Tensor Shape:   ", image.ext_buf[0].shape)
    print("Tensor Strides: ", image.ext_buf[0].strides)
    print("Tensor dType:   ", image.ext_buf[0].dtype)
    print("Tensor Device:  ", image.ext_buf[0].__dlpack_device__(), "\n")
    # ------------
    # GPU Tensor
    # ------------
    # yuv_tensor = torch.from_dlpack(image.ext_buf[0].__dlpack__(image))
    # print(f"Device:\t\t\t {yuv_tensor.device}")
    # print(f"Tensor GPU MEM address:\t {hex(yuv_tensor.data_ptr())}\n")
    # ------------
    #  CPU Tensor
    # ------------
    tensor = torch.from_numpy(image.to_numpy_8bits())
    print(f"Tensor tensor: {tensor.device}")  # Should output: cpu
    print(f"Tensor CPU MEM address:\t {hex(tensor.data_ptr())}\n")
    print("Shape:", tensor.shape)
    print("Dtype:", tensor.dtype)
    # Print top-left 5x5 region (for 2D tensors)
    print(tensor[:5, :5], "\n")

# torch - info - details - debug
print(f"torch.version:\t {torch.version}")
print(f"HIP Version:\t {torch.version.hip}")                # should not be None
print(f"HIP Supported:\t {torch.cuda.is_available()}\n")    # should return True for HIP backend too
line = '\n' + '-' * 40 + '\n'

# //////////////////////////////////
# Example of BATCHED images decode
# //////////////////////////////////
device_id = 0
backend = 0

image_paths = ["/opt/rocm/share/rocjpeg/images/mug_400.jpg", "/opt/rocm/share/rocjpeg/images/mug_420.jpg", "/opt/rocm/share/rocjpeg/images/mug_422.jpg"]
batch_size = len(image_paths)

decoder = jdec.decoder(device_id, backend)

# --------------------------------------------------
# TEST (1) LIST of 'files' (Batch)
# --------------------------------------------------
print(line, "TEST (1) batch of files", line)
images = decoder.decode(image_paths)
for i in range(batch_size):
    printout_tensor_info(images[i])

# --------------------------------------------------
# TEST (2) LIST of 'data' (Batch)
# --------------------------------------------------
print(line, "TEST (2) batch of data", line)
data_list = []
for p in image_paths:
    with open(p, 'rb') as in_file:
        data = in_file.read()
        data_list.append(data)
print("Type of data:", type(data_list))
image_list = decoder.decode(data_list)
for img in image_list:
    printout_tensor_info(img)

# --------------------------------------------------
# TEST (3) LIST of 'arrays' (Batch) numpy arrays
# --------------------------------------------------
print(line, "TEST (3) batch of numpy Array(s)", line)
arrays_list = []
for p in image_paths:
    arrays_list.append(np.fromfile(p, dtype=np.uint8))
images_from_array_list = decoder.decode(arrays_list)
for img in images_from_array_list:
    printout_tensor_info(img)