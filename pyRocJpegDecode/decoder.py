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

    def rocPyJpegDecodeParams(self):
        d_struct = rocpyjpeg.RocJpegDecodeParams()
        d_struct.output_format = jpegt.ROCJPEG_OUTPUT_NATIVE
        d_struct.crop_rectangle.left = 0
        d_struct.crop_rectangle.top = 0
        d_struct.crop_rectangle.right = 0
        d_struct.crop_rectangle.bottom = 0
        d_struct.target_dimension.width = 0
        d_struct.target_dimension.height = 0
        return d_struct

    def PyGetFilePaths(self, input_path, file_paths, is_dir, is_file):
        n_input_path, n_file_paths, n_is_dir, n_is_file = self.jpegdec.PyGetFilePaths(input_path, file_paths, is_dir, is_file)
        return n_input_path, n_file_paths, n_is_dir, n_is_file

    def PyInitHipDevice(self, device_id):
        return self.jpegdec.PyInitHipDevice(device_id)

    def PyGetChromaSubsamplingStr(self, subsampling):
        chroma_string = self.jpegdec.PyGetChromaSubsamplingStr(subsampling)
        return chroma_string