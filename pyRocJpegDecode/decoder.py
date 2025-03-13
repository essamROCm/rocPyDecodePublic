# Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
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

import rocpyjpegdecode as rocpyjpeg
import rocpyjpegdecode.jpegTypes as jpegt
import numpy as np
import ctypes


class PyRocJpegImage(ctypes.Structure):
    _fields_ = [
        ('channel', ctypes.c_uint64 * jpegt.ROCJPEG_MAX_COMPONENT),
        ('pitch', ctypes.c_uint32 * jpegt.ROCJPEG_MAX_COMPONENT),
    ]

def PyRocJpegDecodeParams():
    return rocpyjpeg.RocJpegDecodeParams()

# def PyRocJpegImage():
#     return rocpyjpeg.RocJpegImage()

# rocPyJpeg Decoder Class
class decoder(object):

    def __init__(self):
        self.jpegdec = rocpyjpeg.PyRocJpegDecoder()

    def rocPyJpegCreate(self, backend, device_id):
        return self.jpegdec.rocPyJpegCreate(jpegt.RocJpegBackend(backend), device_id)

    def rocPyJpegStreamCreate(self):
        return self.jpegdec.rocPyJpegStreamCreate()

    def rocPyJpegStreamDestroy(self, jpeg_stream_handle):
        return self.jpegdec.rocPyJpegStreamDestroy(jpeg_stream_handle)

    def rocPyJpegDestroy(self, rocjpeg_handle):
        return self.jpegdec.rocPyJpegDestroy(rocjpeg_handle)

    def rocPyJpegStreamParse(self, file_data, length, jpeg_stream_handle):
        file_array = np.frombuffer(file_data, dtype=np.uint8)
        return self.jpegdec.rocPyJpegStreamParse(file_array, length, jpeg_stream_handle)

    def rocPyJpegGetImageInfo(self, rocjpeg_handle, rocjpeg_stream_handle):
        num_components, subsampling, widths, heights = self.jpegdec.rocPyJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handle)
        return num_components, subsampling, widths, heights

    def rocPyJpegDecode(self, decode_params, rocjpeg_handle, rocjpeg_stream_handle, output_image):
        ctypes.pythonapi.PyCapsule_New.restype = ctypes.py_object
        capsule = ctypes.pythonapi.PyCapsule_New(ctypes.byref(output_image), b'RocJpegImage', None)
        return self.jpegdec.rocPyJpegDecode(decode_params, rocjpeg_handle, rocjpeg_stream_handle, capsule)
