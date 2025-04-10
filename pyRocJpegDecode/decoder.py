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

import rocpyjpegdecode as rocpyjpeg
import rocpyjpegdecode.jpegTypes as jpegt
import numpy as np

class DecodeSource(object):
    def __init__(
            self,
            param):
        self.DS = rocpyjpeg.DecodeSource(param)

    # return the code stream
    def cs(self):
        return self.DS.code_stream

    # return the width of the decoded image
    def width(self):
        return self.DS.width()

    # return the height of the decoded image
    def height(self):
        return self.DS.height()


class decoder(object):
    def __init__(
            self, 
            device_id = 0, 
            backend = 0):
        self.jpegdec = rocpyjpeg.Decoder(device_id, backend)

    # read image or batch of images
    def read(self, img_full_bath):
        self.img = self.jpegdec.read(img_full_bath)
        return self.img        

    # decode image or batch of images
    def decode(self, img_full_bath):
        self.img = self.jpegdec.decode(img_full_bath)
        return self.img
