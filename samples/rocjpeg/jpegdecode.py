import pyRocJpegDecode.decoder as jdec
import pyRocJpegDecode.utils as jutils
import pyRocJpegDecode.types as jpegt
import datetime
import sys
import argparse
import os.path
import ctypes
import time


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

    # roi?
    roi_width = decode_params.crop_rectangle.right - decode_params.crop_rectangle.left
    roi_height = decode_params.crop_rectangle.bottom - decode_params.crop_rectangle.top
    if(roi_width > 0 and roi_height > 0 and roi_width <= widths[0] and roi_height <= heights[0]):
        is_roi_valid = True
        print(f"Cropped image resolution: {roi_width}x{roi_height}")

    # parse input
    file_paths = []
    is_dir = False
    is_file = False
    n_input_path, n_file_paths, n_is_dir, n_is_file = jpegutils.PyGetFilePaths(input_file_path, file_paths, is_dir, is_file)

    # init HIP
    if(jpegutils.PyInitHipDevice(device_id)):
        print("HIP Device Initialized Successfully..\n")

    # init the stream & the codec
    rocjpeg_handle = jpegdecode.rocPyJpegCreate(rocjpeg_backend, device_id)
    rocjpeg_stream_handle = jpegdecode.rocPyJpegStreamCreate()

    # vars-init
    save_images = False if(output_file_path is None) else True
    num_jpegs_with_411_subsampling = 0
    num_jpegs_with_unknown_subsampling = 0
    num_jpegs_with_unsupported_resolution = 0
    num_bad_jpegs = 0
    num_components = 0
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
    # Print to verify
    for i in range(jpegt.ROCJPEG_MAX_COMPONENT):
        print(f"Channel[{i}] Address: {hex(output_image.channel[i])}")
        print(f"Channel Pitch[{i}]: {output_image.pitch[i]}")

    # -------------------------------------------
    # loop to decode images
    # -------------------------------------------
    for file_path in n_file_paths:
        image_count = 0
        base_file_name = os.path.basename(file_path)
        print(base_file_name)

        file_data, file_size = read_image(file_path)

        if file_data is None:
            print(f"Unable to read from {file_path}")
            exit

        print(f"Input file name, size: {file_path}, {file_size}")

        rocjpeg_status = jpegdecode.rocPyJpegStreamParse(file_data, file_size, rocjpeg_stream_handle)
        if (rocjpeg_status != jpegt.ROCJPEG_STATUS_SUCCESS):
            if (is_dir):
                num_bad_jpegs += 1
                continue
            else:
                print(f"ERROR: Failed to parse the input jpeg stream with {rocjpeg_status}")
                exit

        # Get image info
        num_components, subsampling, widths, heights = jpegdecode.rocPyJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handle)

        chroma_sub_sampling = jpegutils.PyGetChromaSubsamplingStr(subsampling)
        print(f"Input image resolution: {widths[0]}x{heights[0]}")
        print(f"Chroma subsampling: {chroma_sub_sampling}")

        if(widths[0] < 64 or heights[0] < 64):
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

        num_channels, channel_sizes = jpegutils.PyGetChannelPitchAndSizes(decode_params, subsampling, widths, heights, output_image)
        if(num_channels <= 0):
            print(f"ERROR: Failed to get the channel pitch and sizes {num_channels}")
            exit
        print(f"channel_sizes: {channel_sizes}")

        # allocate memory for each channel and reuse them if the sizes remain unchanged for a new image.
        for i in range(num_channels):
            if prior_channel_sizes[i] != channel_sizes[i]:
                if output_image.channel[i] is not 0:
                    status = jpegutils.PyFreeHipDeviceMemory(output_image.channel[i])
                    output_image.channel[i] = 0
                status, ptr = jpegutils.PyAllocHipDeviceMemory(channel_sizes[i])
                output_image.channel[i] = ptr
                print(f"---ptr: {ptr} ---STATUS: {status} -- SIZE: {channel_sizes[i]} --output_image.channel[i] = {output_image.channel[i]}")

        print("Decoding started, please wait! ... ")
        start_time = time.time() # Start timing
        print(f"num_channels: {num_channels}")
        print(f"---ptr0: {hex(output_image.channel[0])}")
        print(f"---ptr1: {hex(output_image.channel[1])}")
        status = jpegdecode.rocPyJpegDecode(decode_params, rocjpeg_handle, rocjpeg_stream_handle, output_image) # Call the JPEG decode
        end_time = time.time() # End timing
        time_per_image_in_milli_sec = (end_time - start_time) * 1000 # Compute time per image in milliseconds
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

        print(f"Average processing time per image (ms): {time_per_image_in_milli_sec:.2f}")
        if(time_per_image_in_milli_sec > float(0)):
            print(f"Average images per sec: {1000 / time_per_image_in_milli_sec:.2f}")

        if (is_dir):
            total_images += image_count
            time_per_image_all += time_per_image_in_milli_sec;
            mpixels_all += image_size_in_mpixels

        # Free allocated MEM
        print(f"num_channels: {num_channels}")
        print(f"---ptr0 free: {hex(output_image.channel[0])}")
        print(f"---ptr1 free: {hex(output_image.channel[1])}")

        print(f"Free allocated MEM, channels count: {num_channels}..\n")
        for i in range(num_channels):
            if output_image.channel[i] is not None:
                status = jpegutils.PyFreeHipDeviceMemory(output_image.channel[i])

        if (is_dir):
            time_per_image_all /= total_images  # Compute average time per image
            images_per_sec = 1000 / time_per_image_all  # Compute images per second
            mpixels_per_sec = mpixels_all * images_per_sec / total_images  # Compute megapixels per second
            print(f"Total decoded images: {total_images}")

            # Check if any images were skipped due to errors or unsupported formats
            total_skipped_images = (num_bad_jpegs + num_jpegs_with_411_subsampling +
                                    num_jpegs_with_unknown_subsampling + num_jpegs_with_unsupported_resolution)

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

            # Print performance statistics only if there are processed images
            if total_images:
                print(f"Average processing time per image (ms): {time_per_image_all:.2f}")
                print(f"Average decoded images per sec (Images/Sec): {images_per_sec:.2f}")
                print(f"Average decoded images size (Mpixels/Sec): {mpixels_per_sec:.2f}")

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