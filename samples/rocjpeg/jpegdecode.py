import pyRocJpegDecode.decoder as jdec
import pyRocJpegDecode.types as jpegt
import datetime
import sys
import argparse
import os.path
import ctypes


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

    # init decode_params
    decode_params = jpegdecode.rocPyJpegDecodeParams()

    # format [1:native, 2:yuv_planar, 3:y, 4:rgb, 5:rgb_planar]
    if(output_format is not None):
        if(output_format == 2):
            decode_params.output_format = jpegt.ROCJPEG_OUTPUT_YUV_PLANAR
        else:
            if (output_format == 3):
                decode_params.output_format = jpegt.ROCJPEG_OUTPUT_Y
            else:
                if (output_format == 4):
                    decode_params.output_format = jpegt.ROCJPEG_OUTPUT_RGB
                else:
                    if (output_format == 5):
                        decode_params.output_format = jpegt.ROCJPEG_OUTPUT_RGB_PLANAR

    # crop
    if(crop_rect is not None):
        decode_params.crop_rectangle.left = crop_rect[0]
        decode_params.crop_rectangle.top = crop_rect[1]
        decode_params.crop_rectangle.right = crop_rect[2]
        decode_params.crop_rectangle.bottom = crop_rect[3]

    # roi
    is_roi_valid = False
    roi_width = 0
    roi_height = 0
    roi_width = decode_params.crop_rectangle.right - decode_params.crop_rectangle.left
    roi_height = decode_params.crop_rectangle.bottom - decode_params.crop_rectangle.top

    # init HIP
    if(jpegdecode.PyInitHipDevice(device_id)):
        print("HIP Device Initialized Successfully..\n")

    # init the stream & the codec
    rocjpeg_handle = jpegdecode.rocPyJpegCreate(rocjpeg_backend, device_id)
    rocjpeg_stream_handle = jpegdecode.rocPyJpegStreamCreate()

    # loop to decode images

    num_jpegs_with_411_subsampling = 0
    num_jpegs_with_unknown_subsampling = 0
    num_jpegs_with_unsupported_resolution = 0
    num_bad_jpegs = 0
    num_components = 0
    chroma_sub_sampling = str("")
    subsampling = jpegt.ROCJPEG_CSS_UNKNOWN
    # Create ctypes arrays of uint32_t
    widths = (ctypes.c_uint32 * jpegt.ROCJPEG_MAX_COMPONENT)()
    heights = (ctypes.c_uint32 * jpegt.ROCJPEG_MAX_COMPONENT)()

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

        # Get image info
        num_components, subsampling, widths, heights = jpegdecode.rocPyJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handle)

        # roi?
        if(roi_width > 0 and roi_height > 0 and roi_width <= widths and roi_height <= heights):
            is_roi_valid = True

        chroma_sub_sampling = jpegdecode.PyGetChromaSubsamplingStr(subsampling)
        print(f"Input image resolution: {widths}x{heights}")
        print(f"Chroma subsampling: {chroma_sub_sampling}")

        if(widths < 64 or heights < 64):
            print("The image resolution is not supported by VCN Hardware")
            if (is_dir):
                num_jpegs_with_unsupported_resolution += 1
                continue
            else:
                exit

        if (subsampling == jpegt.ROCJPEG_CSS_411 or subsampling == jpegt.ROCJPEG_CSS_UNKNOWN):
            print("The chroma sub-sampling is not supported by VCN Hardware")
            if (is_dir):
                if (subsampling == jpegt.ROCJPEG_CSS_411):
                    num_jpegs_with_411_subsampling += 1
                if (subsampling == jpegt.ROCJPEG_CSS_UNKNOWN):
                    num_jpegs_with_unknown_subsampling += 1
                continue
            else:
                exit

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