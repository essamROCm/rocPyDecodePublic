import pyRocJpegDecode.decoder as jdec
import datetime
import sys
import argparse
import os.path


def JDecoder(
        input_file_path,
        output_file_path,
        device_id,
        backend,
        output_format,
        crop_rect):

    # TODO: Remove this print when complete coding
    print(f"\nReading images from disk, please wait!\n\
Using GPU device 0: Radeon RX 7900 XT[gfx1100] on PCI bus 03:00.0\n\
Input file name: {input_file_path}\n\
Input image resolution: 3840x2160\n\
Chroma subsampling: YUV 4:2:0\n\
Decoding started, please wait! ...\n\
Average processing time per image (ms): 16.6028\n\
Average images per sec: 60.2309\n\
Decoding completed!\n")
    return


if __name__ == "__main__":

    # get passed arguments
    parser = argparse.ArgumentParser(
        description='PyRocJpegDecode Arguments')
    parser.add_argument(
        '-i',
        '--input',
        type=str,
        help='Input path to a single JPEG image or a directory containing JPEG images - required',
        required=True)
    parser.add_argument(
        '-o',
        '--output',
        type=str,
        help='Path to an output file, or a path to an existing directory - write decoded images to a file or an existing directory based on selected output format - [optional]',
        required=False)
    parser.add_argument(
        '-d',
        '--device',
        type=int,
        default=0,
        help='GPU device ID - optional, default 0, specify the GPU device id for the desired device (use 0 for the first device, 1 for the second device, and so on.',
        required=False)
    parser.add_argument(
        '-be',
        '--backend',
        type=int,
        default=0,
        help='Select rocJPEG backend (0 for hardware-accelerated JPEG decoding using VCN, 1 for hybrid JPEG decoding using CPU and GPU HIP kernels (currently not supported)) [optional - default: 0]',
        required=False)
    parser.add_argument(
        '-crop',
        '--crop_rect',
        nargs=4,
        type=int,
        help='Crop rectangle (left, top, right, bottom), optional, default: no cropping',
        required=False)
    parser.add_argument(
        '-fmt',
        '--output_format',
        type=int,
        default=1,
        help="Select rocJPEG output format for decoding, one of the [1:native, 2:yuv_planar, 3:y, 4:rgb, 5:rgb_planar] - [optional - default: 1:native]",
        required=False)

    try:
        args = parser.parse_args()
    except BaseException:
        sys.exit()

    # get params
    input_file_path = args.input
    output_file_path = args.output
    device_id = args.device
    backend = args.backend
    crop_rect = args.crop_rect
    output_format = args.output_format

    JDecoder(
        input_file_path,
        output_file_path,
        device_id,
        backend,
        output_format,
        crop_rect
        )