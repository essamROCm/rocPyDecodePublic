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

import pyRocJpegDecode.decoder as jdec
import rocpyjpegdecode.jpegTypes as jpegt
import argparse
import ctypes
import os
import sys

_capsule_get_pointer = ctypes.pythonapi.PyCapsule_GetPointer
_capsule_get_pointer.restype = ctypes.c_void_p
_capsule_get_pointer.argtypes = [ctypes.py_object, ctypes.c_char_p]


class _DLDevice(ctypes.Structure):
    _fields_ = [("device_type", ctypes.c_int), ("device_id", ctypes.c_int)]

class _DLDataType(ctypes.Structure):
    _fields_ = [("code", ctypes.c_uint8), ("bits", ctypes.c_uint8), ("lanes", ctypes.c_uint16)]

class _DLTensor(ctypes.Structure):
    _fields_ = [
        ("data", ctypes.c_void_p),
        ("device", _DLDevice),
        ("ndim", ctypes.c_int),
        ("dtype", _DLDataType),
        ("shape", ctypes.POINTER(ctypes.c_int64)),
        ("strides", ctypes.POINTER(ctypes.c_int64)),
        ("byte_offset", ctypes.c_uint64),
    ]

class _DLManagedTensor(ctypes.Structure):
    _fields_ = [
        ("dl_tensor", _DLTensor),
        ("manager_ctx", ctypes.c_void_p),
        ("deleter", ctypes.c_void_p),
    ]


def _dlpack_bytes_and_shape(buffer_obj):
    """Extract bytes, shape, and bit depth from a DLPack buffer without numpy/torch."""
    capsule = buffer_obj.__dlpack__()
    managed_ptr = _capsule_get_pointer(capsule, b"dltensor")
    if not managed_ptr:
        raise RuntimeError("Failed to acquire DLPack pointer from capsule")
    managed = _DLManagedTensor.from_address(managed_ptr)
    tensor = managed.dl_tensor
    shape = [tensor.shape[i] for i in range(tensor.ndim)]
    if not shape:
        raise RuntimeError("Empty shape returned from DLTensor")
    bit_depth = int(tensor.dtype.bits)
    bytes_per_item = max((bit_depth + 7) // 8, 1)
    total_items = 1
    for dim in shape:
        total_items *= dim
    data_ptr = ctypes.c_void_p(tensor.data).value
    if data_ptr is None:
        raise RuntimeError("DLTensor data pointer is null")
    data_ptr += int(tensor.byte_offset)
    raw_bytes = ctypes.string_at(data_ptr, total_items * bytes_per_item)
    return raw_bytes, shape, bit_depth


def _tensor_to_interleaved_rgb(img_tensor, output_format):
    planar_format = int(jpegt.RocJpegOutputFormat.ROCJPEG_OUTPUT_RGB_PLANAR)
    bit_depth = None
    bytes_per_sample = None
    if output_format == planar_format:
        planes = []
        plane_shape = None
        for idx in range(3):
            plane_bytes, shape, bits = _dlpack_bytes_and_shape(img_tensor.ext_buf[idx])
            if len(shape) < 2:
                raise RuntimeError(f"Unexpected plane shape: {shape}")
            if bit_depth is None:
                bit_depth = bits
                bytes_per_sample = max((bit_depth + 7) // 8, 1)
            if plane_shape is None:
                plane_shape = shape
            elif plane_shape[0] != shape[0] or plane_shape[1] != shape[1]:
                raise RuntimeError("RGB planes have mismatched dimensions")
            if bits != bit_depth:
                raise RuntimeError("RGB planes have mismatched bit depths")
            planes.append(plane_bytes)
        height, width = int(plane_shape[0]), int(plane_shape[1])
        pixel_count = height * width
        expected_plane_bytes = pixel_count * bytes_per_sample
        for plane in planes:
            if len(plane) < expected_plane_bytes:
                raise RuntimeError("RGB plane buffer smaller than expected for given dimensions")
        interleaved = bytearray(pixel_count * 3 * bytes_per_sample)
        for i in range(pixel_count):
            base = i * 3 * bytes_per_sample
            offset = i * bytes_per_sample
            interleaved[base:base + bytes_per_sample] = planes[0][offset:offset + bytes_per_sample]
            interleaved[base + bytes_per_sample:base + 2 * bytes_per_sample] = planes[1][offset:offset + bytes_per_sample]
            interleaved[base + 2 * bytes_per_sample:base + 3 * bytes_per_sample] = planes[2][offset:offset + bytes_per_sample]
        return bytes(interleaved), width, height, bit_depth
    else:
        rgb_bytes, shape, bit_depth = _dlpack_bytes_and_shape(img_tensor.ext_buf[0])
        if len(shape) < 3 or shape[2] < 3:
            raise RuntimeError(f"Unexpected tensor shape for RGB output: {shape}")
        height, width = int(shape[0]), int(shape[1])
        bytes_per_sample = max((bit_depth + 7) // 8, 1)
        expected = height * width * 3 * bytes_per_sample
        if len(rgb_bytes) < expected:
            raise RuntimeError("RGB buffer smaller than expected for given dimensions")
        return rgb_bytes[:expected], width, height, bit_depth


def _save_image_from_tensor(img_tensor, output_format, filename):
    base_path = filename.strip()
    dir_name = os.path.dirname(base_path)
    root_name = os.path.splitext(os.path.basename(base_path))[0]
    if not root_name:
        root_name = "output"

    rgb_bytes, width, height, bit_depth = _tensor_to_interleaved_rgb(img_tensor, output_format)
    out_name = f"{root_name}_{width}x{height}_{bit_depth}bits.rgb"
    output_path = os.path.join(dir_name, out_name) if dir_name else out_name

    with open(output_path, "wb") as fh:
        fh.write(rgb_bytes)

    return output_path

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Example of decoding jpeg image
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
def jpeg_decode(
        input_file_path, 
        output_format, 
        device_id,
        backend,
        output_file_path):

    # initialize hip
    devices_count, ret = jdec.initialize_hip(device_id)
    if(ret == False):
        print(f"Exiting jpegdecode application, Device#: {device_id} not found.")
        sys.exit()

    # create the decode instance
    decoder = jdec.decoder(device_id, backend)
    decoder.set_output_image_format(output_format)  # set the output image to the desired format

    print(f"Image output format set to: {jpegt.RocJpegOutputFormat(output_format)}")
    print(f"\nDecoding file: {input_file_path} on Device: {'CPU' if backend else 'GPU'} with Device ID: {device_id}")

    dec_time_msec, img_tensor = decoder.decode(input_file_path)

    print(f"Decoding file: {input_file_path} is complete.\n")

    # example how to save the decoded image as a raw RGB file
    if (output_file_path is not None):
        filename = output_file_path.strip()
        saved_path = _save_image_from_tensor(img_tensor, output_format, filename)
        print(f"Raw RGB image saved as: {saved_path}")


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
    parser.add_argument(
        '-o',
        '--output',
        type=str,
        help='Output File Name Path - optional',
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
    output_file_path = args.output

    if not os.path.isfile(input_file_path):  # Input must be a file
        print("ERROR: input passed with -i must be an existing file.")
        exit()

    jpeg_decode(input_file_path, output_format, device_id, backend, output_file_path)
