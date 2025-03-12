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


# rocPyJpeg Utilities Class
class utils(object):

    def __init__(self):
        self.jpegutils = rocpyjpeg.PyRocJpegUtils()

    def PyGetFilePaths(self, input_path, file_paths, is_dir, is_file):
        n_input_path, n_file_paths, n_is_dir, n_is_file = self.jpegutils.PyGetFilePaths(input_path, file_paths, is_dir, is_file)
        return n_input_path, n_file_paths, n_is_dir, n_is_file

    def PyGetChromaSubsamplingStr(self, subsampling):
        chroma_string = self.jpegutils.PyGetChromaSubsamplingStr(subsampling)
        return chroma_string

    def PyGetChannelPitchAndSizes(self, decode_params, subsampling, widths, heights, output_image):
        return self.jpegutils.PyGetChannelPitchAndSizes(decode_params, subsampling, widths, heights, output_image)

    def PyGetOutputFileExt(self, decode_params, base_file_name, image_width, image_height, subsampling, output_file_name):
        return self.jpegutils.PyGetOutputFileExt(decode_params, base_file_name, image_width, image_height, subsampling, output_file_name)

    def PySaveImage(self, decode_params, image_save_path, width, height, subsampling, output_image):
        return self.jpegutils.PySaveImage(decode_params, image_save_path, width, height, subsampling, output_image)
