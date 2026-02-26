# Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
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

import argparse
import sys
import tempfile
from pathlib import Path

try:
    import numpy as np
except ImportError as exc:  # numpy is required for buffer crafting
    raise SystemExit("numpy is required to run test_rocdecode.py") from exc


def exercise_common_api(rd_module, has_ffmpeg: bool):
    # DLPack end-to-end smoke
    if hasattr(rd_module, "DLPackPyTensor"):
        rd_module.DLPackPyTensor.test_all()
    # Packet builder path
    sample = np.arange(256, dtype=np.uint8)
    pkt = rd_module.GetRocPyDecPacket(1234, len(sample), sample)
    assert pkt.frame_pts == 1234
    assert pkt.bitstream_size == len(sample)
    assert pkt.bitstream_adrs != 0
    # codec helpers (available only when FFmpeg bindings are built)
    if has_ffmpeg and hasattr(rd_module, "AVCodecString2RocDecVideoCodec"):
        rd_module.AVCodecString2RocDecVideoCodec("h264")
        rd_module.AVCodecString2RocDecVideoCodec("hevc")
        rd_module.AVCodecString2RocDecVideoCodec("unknown_codec_name")
    else:
        print("FFmpeg codec helpers not built; skipping codec helper coverage.")


def _decode_single(decoder, demuxer, dec_types, rgb_format, tag: str):
    pkt = demuxer.DemuxFrame()
    if pkt.end_of_stream:
        return
    if hasattr(decoder, "SetReconfigParams"):
        decoder.SetReconfigParams(0, "")
    decoder.DecodeFrame(pkt)
    # hit YUV paths
    decoder.GetFrameYuv(pkt, False)
    decoder.GetFrameYuv(pkt, True)
    # RGB path + BufferInterface members
    decoder.GetFrameRgb(pkt, rgb_format)
    _ = pkt.shapeY
    _ = pkt.strides
    _ = pkt.dtype
    # output surface helpers
    surf_info = decoder.GetOutputSurfaceInfo()
    decoder.GetResizedOutputSurfaceInfo()
    decoder.GetFrameSize()
    decoder.GetStride()
    decoder.GetWidth()
    decoder.GetHeight()
    decoder.GetDeviceinfo()
    decoder.GetBitDepth()
    # resize branch (no-op if dimensions equal)
    try:
        from rocpydecode import Dim  # late import to stay inside module
        resize_dim = Dim()
        resize_dim.w = 64
        resize_dim.h = 36
        decoder.ResizeFrame(pkt, resize_dim, surf_info)
        decoder.GetResizedOutputSurfaceInfo()
    except Exception:
        pass
    # save frame if a surface exists
    tmp_file = Path(tempfile.gettempdir()) / f"rocpydecode_{tag}.bin"
    if pkt.frame_adrs and surf_info:
        decoder.SaveFrameToFile(str(tmp_file), pkt.frame_adrs, surf_info, dec_types.native)
    decoder.GetNumOfFlushedFrames()
    decoder.ReleaseFrame(pkt)


def exercise_decode_paths(rd_module, video_path: Path):
    demuxer = rd_module.PyVideoDemuxer(str(video_path))
    codec_id = demuxer.GetCodecId()
    bit_depth = demuxer.GetBitDepth()
    roc_codec = rd_module.AVCodec2RocDecVideoCodec(codec_id)
    # GPU decoder path
    gpu_dec = rd_module.PyRocVideoDecoder(0, rd_module.decTypes.OUT_SURFACE_MEM_DEV_INTERNAL, roc_codec, False)
    if gpu_dec.IsCodecSupported(0, roc_codec, bit_depth):
        _decode_single(gpu_dec, demuxer, rd_module.decTypes, rd_module.decTypes.rgb, "gpu")
    # CPU decoder path (if compiled in)
    if hasattr(rd_module, "PyRocVideoDecoderCpu"):
        cpu_dec = rd_module.PyRocVideoDecoderCpu(0, rd_module.decTypes.OUT_SURFACE_MEM_HOST_COPIED, roc_codec, False)
        if cpu_dec.IsCodecSupported(0, roc_codec, bit_depth):
            _decode_single(cpu_dec, demuxer, rd_module.decTypes, rd_module.decTypes.rgb, "cpu")
    # demux seeking branch
    demuxer.SeekFrame(0, 1, 0)


def exercise_extra_tests(rd_module, video_path: Path | None):
    # test helper exports exist only when host backend is built
    for fn_name in [
        "TestAll_roc_pybuffer",
        "Test_DLPack",
        "Test_PyReconfigureFlushCallback",
        "Test_CalculateRgbImageSize",
    ]:
        if hasattr(rd_module, fn_name):
            getattr(rd_module, fn_name)()
    if video_path and hasattr(rd_module, "TestAllClassCalls"):
        try:
            rd_module.TestAllClassCalls(str(video_path))
        except Exception:
            # ignore decode-specific failures; coverage was the goal
            pass


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Full-surface coverage driver for rocpydecode")
    parser.add_argument("--video", type=str, required=True, help="Path to H264/H265 sample video")
    args = parser.parse_args(argv)

    video_path = Path(args.video)

    import rocpydecode as rd

    has_ffmpeg = all(
        hasattr(rd, attr) for attr in ["PyVideoDemuxer", "AVCodec2RocDecVideoCodec", "AVCodecString2RocDecVideoCodec"]
    )

    exercise_common_api(rd, has_ffmpeg)
    if has_ffmpeg and video_path:
        exercise_decode_paths(rd, video_path)
    else:
        print("FFmpeg-dependent decode paths not available; skipping demux/decode coverage.")

    exercise_extra_tests(rd, video_path if has_ffmpeg else None)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
