import pyRocJpegDecode.decoder as jdec
import pyRocJpegDecode.utils as jutils
import pyRocJpegDecode.types as jpegt
import datetime
import sys
import argparse
import os.path
import ctypes


def get_format(out_fmt):
    format_mapping = {
        2: jpegt.ROCJPEG_OUTPUT_YUV_PLANAR,
        3: jpegt.ROCJPEG_OUTPUT_Y,
        4: jpegt.ROCJPEG_OUTPUT_RGB,
        5: jpegt.ROCJPEG_OUTPUT_RGB_PLANAR
    }
    return format_mapping.get(out_fmt, jpegt.ROCJPEG_OUTPUT_NATIVE)

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

    # JPEG decode & utils instance
    jpegdecode = jdec.decoder()
    jpegutils = jutils.utils()

    # Create/Init RocJpegDecodeParams
    decode_params = jdec.PyRocJpegDecodeParams()
    decode_params.output_format = get_format(output_format)
    crp_rct = [0,0,0,0]
    if(crop_rect is not None):
        crp_rct = crop_rect
    decode_params.crop_rectangle.left = crp_rct[0]
    decode_params.crop_rectangle.top = crp_rct[1]
    decode_params.crop_rectangle.right = crp_rct[2]
    decode_params.crop_rectangle.bottom = crp_rct[3]
    decode_params.target_dimension.width = 0
    decode_params.target_dimension.height = 0
    is_roi_valid = False

    # parse input
    file_paths, is_dir, status = jpegutils.PyGetFilePaths(input_file_path)
    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
        print(f"Failure - GetFilePaths Status: {status}")

    # init HIP
    status = jpegutils.PyInitHipDevice(device_id)
    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
        print(f"Failure - InitHipDevice Status: {status}")
        sys.exit(1)
    else:
        print("HIP Device Initialized Successfully..\n")

    # init the stream & the codec
    decode_handle, status = jpegdecode.rocPyJpegCreate(rocjpeg_backend, device_id)
    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
        print(f"Failure - JpegDecoderCreate Status: {status}")
        sys.exit(1)

    stream_handle, status = jpegdecode.rocPyJpegStreamCreate()
    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
        print(f"Failure - JpegStreamCreate Status: {status}")
        sys.exit(1)

    # vars-init
    save_images = False if(output_file_path is None) else True
    num_jpegs_with_411_subsampling = 0
    num_jpegs_with_unknown_subsampling = 0
    num_jpegs_with_unsupported_resolution = 0
    num_bad_jpegs = 0
    total_images = 0
    time_per_image_all = 0
    mpixels_all = 0
    chroma_sub_sampling = str("")
    subsampling = jpegt.ROCJPEG_CSS_UNKNOWN
    channel_sizes = [0] * jpegt.ROCJPEG_MAX_COMPONENT
    prior_channel_sizes = [0] * jpegt.ROCJPEG_MAX_COMPONENT

    # image descriptor
    output_image = jdec.PyRocJpegImage()
    # Assign initial values (setting all to zero)
    for i in range(jpegt.ROCJPEG_MAX_COMPONENT):
        output_image.channel[i] = 0
        output_image.pitch[i] = 0

    # -------------------------------------------
    # loop to decode images
    # -------------------------------------------
    for file_path in file_paths:
        image_count = 0
        base_file_name = os.path.basename(file_path)

        file_data, file_size = read_image(file_path)

        if file_data is None:
            print(f"Unable to read from {file_path}")
            sys.exit(1)

        print(f"Input file name, size: {file_path}, {file_size}")

        status = jpegdecode.rocPyJpegStreamParse(file_data, file_size, stream_handle)

        if (status != jpegt.ROCJPEG_STATUS_SUCCESS):
            if (is_dir):
                num_bad_jpegs += 1
                continue
            else:
                print(f"ERROR: Failed to parse the input jpeg stream with {rocjpeg_status}")
                sys.exit(1)

        # Get image info
        subsampling, widths, heights, status = jpegdecode.rocPyJpegGetImageInfo(decode_handle, stream_handle)
        if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
            print(f"Failure - JpegGetImageInfo Status: {status}")
            sys.exit(1)

        # roi?
        roi_width = decode_params.crop_rectangle.right - decode_params.crop_rectangle.left
        roi_height = decode_params.crop_rectangle.bottom - decode_params.crop_rectangle.top
        if(roi_width > 0 and roi_height > 0 and roi_width <= widths[0] and roi_height <= heights[0]):
            is_roi_valid = True
            print(f"Cropped image resolution: {roi_width}x{roi_height}")

        chroma_sub_sampling, status = jpegutils.PyGetChromaSubsamplingStr(subsampling)
        if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
            print(f"Failure - GetChromaSubsamplingStr Status: {status}")
        print(f"Input image resolution: {widths[0]}x{heights[0]}")
        print(f"Chroma subsampling: {chroma_sub_sampling}")

        if(widths[0] < 64 or heights[0] < 64):
            print("The image resolution is not supported by VCN Hardware")
            if (is_dir):
                num_jpegs_with_unsupported_resolution += 1
                continue
            else:
                sys.exit(1)

        if (subsampling == jpegt.ROCJPEG_CSS_411 or subsampling == jpegt.ROCJPEG_CSS_UNKNOWN):
            print("The chroma sub-sampling is not supported by VCN Hardware")
            if (is_dir):
                if (subsampling == jpegt.ROCJPEG_CSS_411):
                    num_jpegs_with_411_subsampling += 1
                if (subsampling == jpegt.ROCJPEG_CSS_UNKNOWN):
                    num_jpegs_with_unknown_subsampling += 1
                continue
            else:
                sys.exit(1)

        num_channels, channel_sizes, status = jpegutils.PyGetChannelPitchAndSizes(decode_params, subsampling, widths, heights, output_image)
        if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
            print(f"Failure - GetChannelPitchAndSizes Status: {status}")

        # allocate memory for each channel and reuse them if the sizes remain unchanged for a new image.
        for i in range(num_channels):
            if prior_channel_sizes[i] != channel_sizes[i]:
                if output_image.channel[i] != 0:
                    status = jpegutils.PyFreeHipDeviceMemory(output_image.channel[i])
                    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
                        print(f"Failure - FreeHipDeviceMemory Status: {status}")
                        sys.exit(1)
                    else:
                        output_image.channel[i] = 0
                ptr, status = jpegutils.PyAllocHipDeviceMemory(channel_sizes[i])
                if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
                    print(f"Failure - AllocHipDeviceMemory Status: {status}")
                    sys.exit(1)
                else:
                    output_image.channel[i] = ptr

        print("Decoding started, please wait! ... ")
        start_time = datetime.datetime.now()

        status = jpegdecode.rocPyJpegDecode(decode_params, decode_handle, stream_handle, output_image) # Call the JPEG decode
        if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
            print(f"Failure - JpegDecode Status: {status}")
            sys.exit(1)

        end_time = datetime.datetime.now()
        time_per_image_in_milli_sec = (end_time - start_time).total_seconds() * 1000 # Compute time per image in milliseconds
        image_size_in_mpixels = (widths[0] * heights[0]) / 1_000_000 # Compute image size in megapixels
        image_count += 1 # Increment image count

        for i in range(jpegt.ROCJPEG_MAX_COMPONENT):
            prior_channel_sizes[i] = channel_sizes[i]

        if (save_images):
            image_save_path = output_file_path
            # if ROI is present, need to pass roi_width and roi_height
            width = roi_width if(is_roi_valid) else widths[0]
            height = roi_height if(is_roi_valid) else heights[0]
            if (is_dir):
                image_save_path = jpegutils.PyGetOutputFileExt(decode_params, base_file_name, width, height, subsampling, output_file_path)
            jpegutils.PySaveImage(decode_params, image_save_path, width, height, subsampling, output_image)
            print(f"output file name: {image_save_path}, width x height: {width} x {height}")

        print(f"Average processing time per image (ms): {time_per_image_in_milli_sec:.2f}")
        if(time_per_image_in_milli_sec > float(0)):
            print(f"Average images per sec: {1000 / time_per_image_in_milli_sec:.2f}")

        if (is_dir):
            total_images += image_count
            time_per_image_all += time_per_image_in_milli_sec;
            mpixels_all += image_size_in_mpixels

        if (is_dir):
            time_per_image_all /= total_images  # Compute average time per image
            images_per_sec = 1000 / time_per_image_all  # Compute images per second
            mpixels_per_sec = mpixels_all * images_per_sec / total_images  # Compute megapixels per second
            print(f"Total decoded images: {total_images}")

            # Check if any images were skipped due to errors or unsupported formats
            total_skipped_images = (num_bad_jpegs + num_jpegs_with_411_subsampling + num_jpegs_with_unknown_subsampling + num_jpegs_with_unsupported_resolution)
            if (total_skipped_images):
                print(f"Total skipped images: {total_skipped_images}", end="")
                if num_bad_jpegs:
                    print(f", total images that cannot be parsed: {num_bad_jpegs}", end="")
                if num_jpegs_with_411_subsampling:
                    print(f", total images with YUV 4:1:1 chroma subsampling: {num_jpegs_with_411_subsampling}", end="")
                if num_jpegs_with_unknown_subsampling:
                    print(f", total images with unknown chroma subsampling: {num_jpegs_with_unknown_subsampling}", end="")
                if num_jpegs_with_unsupported_resolution:
                    print(f", total images with unsupported resolution: {num_jpegs_with_unsupported_resolution}", end="")
                print()  # Newline

            # Performance statistics if there are processed images
            if total_images:
                print(f"Average processing time per image (ms): {time_per_image_all:.3f}")
                print(f"Average decoded images per sec (Images/Sec): {images_per_sec:.3f}")
                print(f"Average decoded images size (Mpixels/Sec): {mpixels_per_sec:.3f}")
    # end
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

    # validate params
    if not os.path.exists(input_file_path): # Input must exist
        print("ERROR: input file doesn't exist.")
        sys.exit(1)
    if os.path.isdir(input_file_path): # if in dir
        if output_file_path is not None:
            if not os.path.isdir(output_file_path): # out must be dir
                print("ERROR: for passing folder path as input, you must pass an existing folder path as output.")
                sys.exit(1)
    if(device_id < 0):
        print(f"Arg Error: Invalid device ID: {device_id}\n")
        sys.exit(1)
    if(rocjpeg_backend < 0):
        print(f"Arg Error: Invalid back end: {rocjpeg_backend}\n")
        sys.exit(1)
    if(output_format < 1 or output_format > 5):
        print(f"Arg Error: Invalid output format: {output_format}\n")
        sys.exit(1)

    JDecoder(
        input_file_path,
        output_file_path,
        device_id,
        rocjpeg_backend,
        output_format,
        crop_rect
        )