import pyRocJpegDecode.decoder as jdec
import pyRocJpegDecode.utils as jutils
import pyRocJpegDecode.types as jpegt
import argparse
import datetime
import os
import sys
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


def JDecoderBatched(
        input_dir,
        output_path,
        device_id,
        rocjpeg_backend,
        output_format,
        crop_rect,
        in_batch_size):

    # init variables
    total_images = 0
    batch_size = 2 if(in_batch_size is None or in_batch_size <= 0) else in_batch_size
    time_per_image_all = float(0)
    mpixels_all = float(0)
    images_per_sec = float(0)
    time_per_batch_in_milli_sec = float(0)
    num_bad_jpegs = 0
    num_jpegs_with_411_subsampling = 0
    num_jpegs_with_unknown_subsampling = 0
    num_jpegs_with_unsupported_resolution = 0

    # Instances of decoder & utils
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

    # HIP device init
    status = jpegutils.PyInitHipDevice(device_id)
    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
        print(f"Failure - InitHipDevice Status: {status}")
        sys.exit(1)
    else:
        print("HIP Device Initialized Successfully..\n")

    # Create decode handle
    rocjpeg_handle, status = jpegdecode.rocPyJpegCreate(rocjpeg_backend, device_id)
    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
        print(f"Failure - JpegDecoderCreate Status: {status}")
        sys.exit(1)

    # Get list of files
    file_paths, is_dir, status = jpegutils.PyGetFilePaths(input_dir)
    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
        print(f"Failure - GetFilePaths Status: {status}")

    batch_size = min(batch_size, len(file_paths))

    # Prepare batch structures
    rocjpeg_stream_handles = []
    for i in range(batch_size):
        handle, status = jpegdecode.rocPyJpegStreamCreate()
        rocjpeg_stream_handles.append(handle)
        if status != jpegt.ROCJPEG_STATUS_SUCCESS:
            print(f"Failure - JpegStreamCreate Status at index {i}: {status}")
            sys.exit(1)

    # Helper data for reuse
    output_images = jdec.PyRocJpegImageArray(batch_size)
    decode_params_batch = [decode_params] * batch_size
    prior_channel_sizes = [[0] * jpegt.ROCJPEG_MAX_COMPONENT for _ in range(batch_size)]
    rocjpeg_stream_handles_for_current_batch = []
    base_name_list = []
    widths_list = []
    heights_list = []
    subsampling_list = []
    current_batch_size = 0

    # Iterate over files in batches
    for i in range(0, len(file_paths), batch_size):
        batch_end = min(i + batch_size, len(file_paths));
        for j in range(i, batch_end):
            index = j - i
            temp_base_file_name = os.path.basename(file_paths[j])

            # Read an image from disk (get the size)
            file_data, file_size = read_image(file_paths[j])

            # Parse stream
            status = jpegdecode.rocPyJpegStreamParse(file_data, file_size, rocjpeg_stream_handles[index])
            if (status != jpegt.ROCJPEG_STATUS_SUCCESS):
                if (is_dir):
                    num_bad_jpegs += 1
                    print(f"Skipping decoding input file: {file_paths[j]}")
                    continue
                else:
                    print(f"ERROR: Failed to parse the input jpeg stream with: {status}")
                    sys.exit(1)

            # Get image info
            temp_subsampling, temp_widths, temp_heights, status = jpegdecode.rocPyJpegGetImageInfo(rocjpeg_handle, rocjpeg_stream_handles[index])
            if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
                print(f"Failure - JpegGetImageInfo Status: {status}")
                sys.exit(1)

            if (temp_widths[0] < 64 or temp_heights[0] < 64):
                if (is_dir):
                    num_jpegs_with_unsupported_resolution += 1
                    continue
                else:
                    print("The image resolution is not supported by VCN Hardware", file=sys.stderr)
                    sys.exit(1)

            if (temp_subsampling == jpegt.ROCJPEG_CSS_411 or temp_subsampling == jpegt.ROCJPEG_CSS_UNKNOWN):
                if (is_dir):
                    if (temp_subsampling == jpegt.ROCJPEG_CSS_411):
                        num_jpegs_with_411_subsampling += 1
                    if (temp_subsampling == jpegt.ROCJPEG_CSS_UNKNOWN):
                        num_jpegs_with_unknown_subsampling += 1
                    continue
                else:
                    print("The chroma sub-sampling is not supported by VCN Hardware", file=sys.stderr)
                    sys.exit(1)

            # Get channel sizes & allocate memory
            num_channels, channel_sizes, status = jpegutils.PyGetChannelPitchAndSizes(decode_params_batch[index], temp_subsampling, temp_widths, temp_heights, output_images[current_batch_size])
            if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
                print(f"Failure - GetChannelPitchAndSizes Status: {status}")
                sys.exit(1)

            # alloc for each channel
            for ch in range(num_channels):
                if prior_channel_sizes[current_batch_size][ch] != channel_sizes[ch]:
                    if output_images[current_batch_size].channel[ch] != 0:
                        status = jpegutils.PyFreeHipDeviceMemory(output_images[current_batch_size].channel[ch])
                        if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
                            print(f"Failure - FreeHipDeviceMemory Status: {status}")
                    ptr, status = jpegutils.PyAllocHipDeviceMemory(channel_sizes[ch])
                    if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
                        print(f"Failure - AllocHipDeviceMemory Status: {status}")
                    else:
                        output_images[current_batch_size].channel[ch] = ptr
                        prior_channel_sizes[current_batch_size][ch] = channel_sizes[ch]

            # Prepare for batched call
            rocjpeg_stream_handles_for_current_batch.append(rocjpeg_stream_handles[index])
            subsampling_list.append(temp_subsampling)
            widths_list.append(temp_widths)
            heights_list.append(temp_heights)
            base_name_list.append(temp_base_file_name)
            current_batch_size += 1

        # Batched Decode
        time_per_batch_in_milli_sec = float(0)
        if(current_batch_size > 0):
            start_time = datetime.datetime.now()
            status = jpegdecode.rocPyJpegDecodeBatched(rocjpeg_handle, rocjpeg_stream_handles_for_current_batch, current_batch_size, decode_params_batch, output_images)
            end_time = datetime.datetime.now()
            time_per_batch_in_milli_sec = (end_time - start_time).total_seconds() * 1000.0
            if(status != jpegt.ROCJPEG_STATUS_SUCCESS):
                print(f"Failure - PyJpegDecodeBatched Status: {status}")
                sys.exit(1)

        image_size_in_mpixels = float(0)
        for b in range(current_batch_size):
            image_size_in_mpixels += (float(widths_list[b][0]) * float(heights_list[b][0]) / 1_000_000)

        total_images += current_batch_size

        # Save Images
        if output_path:
            for b in range(current_batch_size):
                output_file_name = output_path
                # if ROI is present, need to pass roi_width and roi_height
                roi_width = decode_params_batch[b].crop_rectangle.right - decode_params_batch[b].crop_rectangle.left
                roi_height = decode_params_batch[b].crop_rectangle.bottom - decode_params_batch[b].crop_rectangle.top
                is_roi_valid = True if(roi_width > 0 and roi_height > 0 and roi_width <= widths_list[b][0] and roi_height <= heights_list[b][0]) else False
                width = roi_width if(is_roi_valid) else widths_list[b][0]
                height = roi_height if(is_roi_valid) else heights_list[b][0]
                if(is_dir):
                    output_file_name = jpegutils.PyGetOutputFileExt(decode_params_batch[b], base_name_list[b], width, height, subsampling_list[b], output_path)
                jpegutils.PySaveImage(decode_params_batch[b], output_file_name, width, height, subsampling_list[b], output_images[b])

        if(is_dir):
            time_per_image_all = time_per_image_all + time_per_batch_in_milli_sec
            mpixels_all = mpixels_all + image_size_in_mpixels

        current_batch_size = 0
        base_name_list.clear()
        widths_list.clear()
        heights_list.clear()
        subsampling_list.clear()
        rocjpeg_stream_handles_for_current_batch.clear()

    if is_dir:
        time_per_image_all = time_per_image_all / total_images
        images_per_sec = 1000 / time_per_image_all
        mpixels_per_sec = mpixels_all * images_per_sec / total_images
        print(f"Total decoded images: {total_images}")

        if (num_bad_jpegs or num_jpegs_with_411_subsampling or num_jpegs_with_unknown_subsampling or num_jpegs_with_unsupported_resolution):
            skipped_total = (num_bad_jpegs + num_jpegs_with_411_subsampling + num_jpegs_with_unknown_subsampling + num_jpegs_with_unsupported_resolution)
            print(f"Total skipped images: {skipped_total}", end='')
            if num_bad_jpegs:
                print(f" ,total images that cannot be parsed: {num_bad_jpegs}", end='')
            if num_jpegs_with_411_subsampling:
                print(f" ,total images with YUV 4:1:1 chroma temp_subsampling: {num_jpegs_with_411_subsampling}", end='')
            if num_jpegs_with_unknown_subsampling:
                print(f" ,total images with unknown chroma temp_subsampling: {num_jpegs_with_unknown_subsampling}", end='')
            if num_jpegs_with_unsupported_resolution:
                print(f" ,total images with unsupported resolution: {num_jpegs_with_unsupported_resolution}", end='')
            print()  # Final newline
        if total_images:
            print(f"Average processing time per image (ms): {time_per_image_all}")
            print(f"Average decoded images per sec (Images/Sec): {images_per_sec}")
            print(f"Average decoded images size (Mpixels/Sec): {mpixels_per_sec}")

    print("\nBatched JPEG decoding completed.\n")


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
        help='Path to an existing directory or will be created - write decoded images to a directory based on selected output format - [optional]',
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
        '-fmt',
        '--output_format',
        type=int,
        default=1,
        help="Select rocJPEG output format for decoding, one of the [1:native, 2:yuv_planar, 3:y, 4:rgb, 5:rgb_planar] - [optional - default: 1:native]",
        required=False)
    parser.add_argument(
        '-bs', 
        '--batch_size',
        type=int,
        default=1,
        help='Batch size for decoding, must pass the batch size desired, default is 1 - required',
        required=True)
    parser.add_argument(
        '-crop',
        '--crop_rect',
        nargs=4,
        type=int,
        help='Crop rectangle (left, top, right, bottom), optional, default: no cropping',
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
    output_format = args.output_format
    batch_size = args.batch_size
    crop_rect = args.crop_rect

    # validate params
    if not os.path.exists(input_file_path):  # Input must exist
        print("ERROR: input file doesn't exist.")
        sys.exit()
    if(batch_size <= 0):
        print(f"Arg Error: Invalid batch size: -bs {batch_size}\n")
        sys.exit()
    if(device_id < 0):
        print(f"Arg Error: Invalid device ID: {device_id}\n")
        sys.exit()
    if(rocjpeg_backend < 0):
        print(f"Arg Error: Invalid back end: {rocjpeg_backend}\n")
        sys.exit()
    if(output_format < 1 or output_format > 5):
        print(f"Arg Error: Invalid output format: {output_format}\n")
        sys.exit()
    if(output_file_path is not None):
        if(not os.path.exists(output_file_path) or not os.path.isdir(output_file_path)):
            print("Warning: output folder specified doesn't exist, no image(s) will be saved.")
            output_file_path = None # decode with no attempt to save

    JDecoderBatched(
        input_file_path,
        output_file_path,
        device_id,
        rocjpeg_backend,
        output_format,
        crop_rect,
        batch_size
        )