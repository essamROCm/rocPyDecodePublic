
import pyRocJpegDecode.decoder as jdec
import pyRocJpegDecode.utils as jutils
import pyRocJpegDecode.types as jpegt
import datetime
import sys
import argparse
import os.path


def get_format(out_fmt):
    format_mapping = {
        2: jpegt.ROCJPEG_OUTPUT_YUV_PLANAR,
        3: jpegt.ROCJPEG_OUTPUT_Y,
        4: jpegt.ROCJPEG_OUTPUT_RGB,
        5: jpegt.ROCJPEG_OUTPUT_RGB_PLANAR
    }
    return format_mapping.get(out_fmt, jpegt.ROCJPEG_OUTPUT_NATIVE)


def read_image(file_path):
    if not os.path.exists(file_path):
        print(f"ERROR: Cannot open image: {file_path}", file=sys.stderr)
        return None
    with open(file_path, "rb") as f:
        return f.read()


def JDecoderBatched(
        input_file_path, 
        output_file_path, 
        device_id, 
        rocjpeg_backend, 
        output_format, 
        batch_size):
    
    # JPEG decode & utils instance
    jpegdecode = jdec.decoder()
    jpegutils = jutils.utils()
    
    # Create/Init RocJpegDecodeParams
    decode_params = jdec.PyRocJpegDecodeParams()
    decode_params.output_format = get_format(output_format)

    # parse input
    file_paths, is_dir, is_file = jpegutils.PyGetFilePaths(input_file_path, [], False, False)[1:4]

    # init HIP
    if not jpegutils.PyInitHipDevice(device_id):
        print("ERROR: Failed to initialize HIP!", file=sys.stderr)
        return

    # init the stream & the codec - output images array
    decode_handle = jpegdecode.rocPyJpegCreate(rocjpeg_backend, device_id)
    batch_size = min(batch_size, len(file_paths))

    stream_handles = [jpegdecode.rocPyJpegStreamCreate() for _ in range(batch_size)]
    output_images = jdec.PyRocJpegImageArray(batch_size)

    total_images, mpixels_all, time_per_image_all = 0, 0, 0

    print("Decoding started, please wait...")

    for i in range(0, len(file_paths), batch_size):
        current_files = file_paths[i:i + batch_size]
        current_batch_size = len(current_files)
        widths, heights, subsamplings = [], [], []

        for idx, file_path in enumerate(current_files):
            file_data = read_image(file_path)
            jpegdecode.rocPyJpegStreamParse(file_data, len(file_data), stream_handles[idx])
            num_components, subsampling, w, h = jpegdecode.rocPyJpegGetImageInfo(decode_handle, stream_handles[idx])
            subsamplings.append(subsampling)
            widths.append(w)
            heights.append(h)
            num_channels, channel_sizes = jpegutils.PyGetChannelPitchAndSizes(decode_params, subsampling, w, h, output_images[idx])

            for ch_idx in range(num_channels):
                _, ptr = jpegutils.PyAllocHipDeviceMemory(channel_sizes[ch_idx])
                output_images[idx].channel[ch_idx] = ptr

        start_time = datetime.datetime.now()
        jpegdecode.rocPyJpegDecodeBatched(decode_handle, stream_handles[:current_batch_size], current_batch_size, decode_params, output_images)
        end_time = datetime.datetime.now()

        time_taken = (end_time - start_time).total_seconds() * 1000
        time_per_image_all += time_taken
        for w, h in zip(widths, heights):
            mpixels_all += (w[0] * h[0]) / 1_000_000
        total_images += current_batch_size

        if output_file_path:
            for idx in range(current_batch_size):
                save_path = output_file_path
                jpegutils.PySaveImage(decode_params, save_path, widths[idx][0], heights[idx][0], subsamplings[idx], output_images[idx])
                print(f"Saved: {save_path}")

        print(f"Batch of {current_batch_size} images processed in {time_taken:.2f} ms.")

    # Performance statistics if there are processed images
    if total_images:
        avg_time = time_per_image_all / total_images
        images_per_sec = 1000 / avg_time
        mpixels_per_sec = mpixels_all * images_per_sec / total_images
        print(f"Total decoded images: {total_images}")
        print(f"Average processing time per image (ms): {avg_time:.2f}")
        print(f"Average images/sec: {images_per_sec:.2f}")
        print(f"Average Mpixels/sec: {mpixels_per_sec:.2f}")

    for idx in range(batch_size):
        for ch in output_images[idx].channel:
            if ch != 0:
                jpegutils.PyFreeHipDeviceMemory(ch)

    print("Batch Decoding completed.")


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

    JDecoderBatched(
        input_file_path,
        output_file_path,
        device_id,
        rocjpeg_backend,
        output_format,
        batch_size
        )