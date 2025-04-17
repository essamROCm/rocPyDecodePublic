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
import rocpyjpegdecode.jpegTypes as jpegt
import argparse
import os
import sys

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Example of decoding jpeg image
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
def jpeg_decode(
        input_file_path, 
        output_format, 
        device_id,
        backend):

    # create the decode instance
    decoder = jdec.decoder(device_id, backend)
    decoder.set_output_image_format(output_format)  # set the output image to the desired format

    print(f"Image output format set to: {jpegt.RocJpegOutputFormat(output_format)}")

    print(f"\nDecoding file: {input_file_path} on Device: {'CPU' if backend else 'GPU'} with Device ID: {device_id}")

    img = decoder.decode(input_file_path)

    if(img.is_valid()):
        yuv_tensor = torch.from_dlpack(img.ext_buf[0].__dlpack__(img))
        print(f"Device:\t\t\t {yuv_tensor.device}")
        print(f"Tensor GPU MEM address:\t {hex(yuv_tensor.data_ptr())}\n")
        tensor = torch.from_numpy(img.to_numpy())
        print("Shape:", tensor.shape)
        print("Dtype:", tensor.dtype)
        print(tensor[:5, :5], "\n")
    else:
        print("Invalid image returned..")    
    print(f"Decoding file: {input_file_path} is complete.\n")


if __name__ == "__main__":

    # get passed arguments
    parser = argparse.ArgumentParser(
        description='Jpeg decode example Arguments')
    parser.add_argument(
        '-i',
        '--input',
        type=str,
        help='Input File-FULL-Path - required',
        required=True)
    parser.add_argument(
        '-fmt',
        '--output_format',
        type=int,
        choices=[3, 4],
        default=3,
        help='Set output image format: 3 for ROCJPEG_OUTPUT_RGB (interleaved), 4 for ROCJPEG_OUTPUT_RGB_PLANAR. Optional, default is 3.',
        required=False)
    parser.add_argument(
        '-bk',
        '--backend',
        type=int,
        choices=[0, 1],
        default=0,
        help='Set backend choice 0:GPU and 1:CPU, Optional, default is 0',
        required=False)    
    parser.add_argument(
        '-d',
        '--device',
        type=int,
        default=0,
        help='GPU device ID - optional, default 0',
        required=False)

    try:
        args = parser.parse_args()
    except BaseException:
        sys.exit()

    # get params
    input_file_path = args.input
    output_format = args.output_format
    device_id = args.device
    backend = args.backend

    if not os.path.exists(input_file_path):  # Input file or folder (must exist)
        print("ERROR: input file doesn't exist.")
        exit()

    jpeg_decode(input_file_path, output_format, device_id, backend)