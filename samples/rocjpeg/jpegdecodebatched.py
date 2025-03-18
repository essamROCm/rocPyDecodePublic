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
        batch_size):

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
    jpegutils.PyInitHipDevice(device_id)

    # Create decode handle
    decode_handle = jpegdecode.rocPyJpegCreate(rocjpeg_backend, device_id)

    # Get list of files
    input_path, file_paths, is_dir, is_file = jpegutils.PyGetFilePaths(input_dir, [], False, False)
    total_files = len(file_paths)
    batch_size = min(batch_size, total_files)

    # Prepare batch structures
    stream_handles = [jpegdecode.rocPyJpegStreamCreate() for _ in range(batch_size)]
    output_images = jdec.PyRocJpegImageArray(batch_size)
    decode_params_batch = [decode_params] * batch_size

    # Helper data for reuse
    prior_channel_sizes = [[0] * jpegt.ROCJPEG_MAX_COMPONENT for _ in range(batch_size)]

    # Iterate over files in batches
    for i in range(0, total_files, batch_size):
        current_batch_files = file_paths[i:i + batch_size]
        stream_handles_batch = []
        widths_list = []
        heights_list = []
        subsampling_list = []

        for idx, file_path in enumerate(current_batch_files):
            # Read image
            file_data, file_size = read_image(file_path)

            # Parse stream
            jpegdecode.rocPyJpegStreamParse(file_data, file_size, stream_handles[idx])

            # Get image info
            _, subsampling, widths, heights = jpegdecode.rocPyJpegGetImageInfo(decode_handle, stream_handles[idx])

            # Get channel sizes & allocate memory
            num_channels, channel_sizes = jpegutils.PyGetChannelPitchAndSizes(decode_params_batch[idx], subsampling, widths, heights, output_images[idx])

            # alloc for each channel
            for ch in range(num_channels):
                if prior_channel_sizes[idx][ch] != channel_sizes[ch]:
                    if output_images[idx].channel[ch] != 0:
                        jpegutils.PyFreeHipDeviceMemory(output_images[idx].channel[ch])
                    status, ptr = jpegutils.PyAllocHipDeviceMemory(channel_sizes[ch])
                    output_images[idx].channel[ch] = ptr
                    prior_channel_sizes[idx][ch] = channel_sizes[ch]

            # Prepare for batched call
            widths_list.append(widths)
            heights_list.append(heights)
            subsampling_list.append(subsampling)
            stream_handles_batch.append(stream_handles[idx])

        # Batched Decode
        start_time = datetime.datetime.now()
        jpegdecode.rocPyJpegDecodeBatched(decode_handle, stream_handles_batch, len(current_batch_files), decode_params_batch, output_images)
        end_time = datetime.datetime.now()

        time_per_batch = (end_time - start_time).total_seconds() * 1000
        print(f"Batch of {len(current_batch_files)} images decoded in {time_per_batch:.2f} ms")

        # Save Images
        if output_path:
            for idx, file_path in enumerate(current_batch_files):
                base_name = os.path.basename(file_path)
                # if ROI is present, need to pass roi_width and roi_height
                roi_width = decode_params_batch[idx].crop_rectangle.right - decode_params_batch[idx].crop_rectangle.left
                roi_height = decode_params_batch[idx].crop_rectangle.bottom - decode_params_batch[idx].crop_rectangle.top
                is_roi_valid = True if(roi_width > 0 and roi_height > 0 and roi_width <= widths_list[idx][0] and roi_height <= heights_list[idx][0]) else False
                width = roi_width if(is_roi_valid) else widths_list[idx][0]
                height = roi_height if(is_roi_valid) else heights_list[idx][0]
                if(is_dir):
                    output_file_name = jpegutils.PyGetOutputFileExt(decode_params_batch[idx], base_name, width, height, subsampling_list[idx], output_path)
                jpegutils.PySaveImage(decode_params_batch[idx], output_file_name, width, height, subsampling_list[idx], output_images[idx])
                print(f"Saved: {output_file_name}")

        # Free Allocated Memory
        for idx in range(len(current_batch_files)):
            for ch in range(jpegt.ROCJPEG_MAX_COMPONENT):
                if output_images[idx].channel[ch] != 0:
                    jpegutils.PyFreeHipDeviceMemory(output_images[idx].channel[ch])

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
        help='Path to an existing directory - write decoded images to an existing directory based on selected output format - [optional]',
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
        default=2,
        help='Batch size for decoding',
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

    JDecoderBatched(
        input_file_path,
        output_file_path,
        device_id,
        rocjpeg_backend,
        output_format,
        crop_rect,
        batch_size
        )