import pyRocJpegDecode.decoder as jdec
import pyRocJpegDecode.types as jpegt
import datetime
import sys
import argparse
import os.path


def read_image(file_path):
    # Check if the file exists
    if not os.path.exists(file_path):
        print(f"ERROR: Cannot open image: {file_path}", file=sys.stderr)
        return None, 0
    try:
        # Open file in binary mode
        with open(file_path, "rb") as input_file:
            # Get file size
            input_file.seek(0, os.SEEK_END)
            file_size = input_file.tell()
            input_file.seek(0)
            # Read file content
            file_data = input_file.read(file_size)
        return file_data, file_size
    except Exception as e:
        print(f"ERROR: Cannot read from file: {file_path}\n{e}", file=sys.stderr)
        return None, 0


def JDecoder(
        input_file_path,
        output_file_path,
        device_id,
        rocjpeg_backend,
        output_format,
        crop_rect):

    # JPEG decode instance
    jpegdecode = jdec.decoder()

    # parse input
    file_paths = []
    is_dir = False
    is_file = False
    n_input_path, n_file_paths, n_is_dir, n_is_file = jpegdecode.PyGetFilePaths(input_file_path, file_paths, is_dir, is_file)

    # init HIP
    if(jpegdecode.PyInitHipDevice(device_id)):
        print("HIP Device Initialized Successfully..\n")

    # init the stream & the codec
    rocjpeg_handle = jpegdecode.rocPyJpegCreate(rocjpeg_backend, device_id)
    rocjpeg_stream_handle = jpegdecode.rocPyJpegStreamCreate()

    # loop to decode images

    num_bad_jpegs = 0
    num_components = 0
    subsampling = jpegt.ROCJPEG_CSS_UNKNOWN
    widths = 0
    heights = 0
    decode_params = jpegdecode.rocPyJpegDecodeParams()

    for file_path in n_file_paths:
        image_count = 0
        base_file_name = os.path.basename(file_path)
        print(base_file_name)

        file_data, file_size = read_image(file_path)

        if file_data is None:
            print(f"Unable to read from {file_path}")
            exit

        print(f"Input file name: {file_path}")

        rocjpeg_status = jpegdecode.rocPyJpegStreamParse(file_data, file_size, rocjpeg_stream_handle)

        #print(f"rocjpeg_status: {rocjpeg_status}")
        if (rocjpeg_status != jpegt.ROCJPEG_STATUS_SUCCESS):
            if (is_dir):
                num_bad_jpegs += 1
                continue
            else:
                print(f"ERROR: Failed to parse the input jpeg stream with {rocjpeg_status}")
                exit

        num_components, subsampling, widths, heights = jpegdecode.rocPyJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handle)

# if (roi_width > 0 && roi_height > 0 && roi_width <= widths[0] && roi_height <= heights[0]) {
# is_roi_valid = true;
# }

    # end
    jpegdecode.rocPyJpegDestroy(rocjpeg_handle)
    jpegdecode.rocPyJpegStreamDestroy(rocjpeg_stream_handle)
    print("\nDecoding completed!\n")

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
    rocjpeg_backend = args.backend
    crop_rect = args.crop_rect
    output_format = args.output_format

    JDecoder(
        input_file_path,
        output_file_path,
        device_id,
        rocjpeg_backend,
        output_format,
        crop_rect
        )