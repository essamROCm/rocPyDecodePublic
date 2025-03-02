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
import rocpyjpegdecode.jpegTypes as jpegtypes
# import numpy as np


# def GetOutputFormat(rgb_format) -> dectypes.OutputFormatEnum:
#     out_format = dectypes.OutputFormatEnum(rgb_format)
#     return out_format

# def GetRocDecCodecID(codec_id) -> dectypes.rocDecVideoCodec:
#     rocCodecId = None
#     if isinstance(codec_id, int):
#         rocCodecId = rocpyjpeg.AVCodec2RocDecVideoCodec(codec_id)
#     if isinstance(codec_id, str):
#         rocCodecId = rocpyjpeg.AVCodecString2RocDecVideoCodec(codec_id)
#     return rocCodecId

# def GetRectangle(crop_rect: dict) -> rocpyjpeg.Rect:
#     p_crop_rect = rocpyjpeg.Rect()
#     if (crop_rect is not None):
#         p_crop_rect.left = crop_rect[0]
#         p_crop_rect.top = crop_rect[1]
#         p_crop_rect.right = crop_rect[2]
#         p_crop_rect.bottom = crop_rect[3]
#     return p_crop_rect

# def GetDim(p_dim_wd: dict) -> rocpyjpeg.Dim:
#     dim_wd = rocpyjpeg.Dim()
#     if (p_dim_wd is not None):
#         dim_wd.width = p_dim_wd[0]
#         dim_wd.height = p_dim_wd[1]
#     return dim_wd

# def GetOutputSurfaceInfo():
#     surf_info_struct = rocpyjpeg.OutputSurfaceInfo()
#     return surf_info_struct

# def GetrocpyjpegPacket(pts, size, buffer):
#     #pts_us = int(pts * 1000 * 1000)  #TBD: if needed in microseconds
#     return rocpyjpeg.GetrocpyjpegPacket(0 if pts == None else int(pts), size, buffer)

class decoder(object):
    # def __init__(
    #         self,
    #         codec,
    #         device_id = 0,
    #         mem_type = 0,
    #         b_force_zero_latency = False,
    #         crop_rect = None,
    #         max_width = 0,
    #         max_height = 0,
    #         clk_rate = 1000):
    #     p_crop_rect = GetRectangle(crop_rect)
    #     self.viddec = rocpyjpeg.PyRocVideoDecoder(
    #         device_id,
    #         mem_type,
    #         codec,
    #         b_force_zero_latency,
    #         p_crop_rect,
    #         max_width,
    #         max_height,
    #         clk_rate)
