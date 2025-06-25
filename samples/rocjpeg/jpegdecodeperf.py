# Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
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

import pyRocJpegDecode.decoder as jdec
import argparse
import os
import sys
import datetime
import multiprocessing
from multiprocessing import Process, Value

# decoding batch of jpeg images
def jpeg_decode_batch_process(
        files_batch_full_path_list,
        batch_size,
        output_format,
        device_id,
        backend,
        images_total,
        bad_images,
        mps,
        ips):

    # initialize hip
    if(jdec.initialize_hip(device_id) == False):
        print(f"Exiting jpegdecodeperf application, Device#: {device_id} not found.")
        sys.exit()
    # create the decoder instance
    decoder = jdec.decoder(device_id, backend)
    decoder.set_output_image_format(output_format)   
    total = len(files_batch_full_path_list)
    total_valid_images_processed = 0
    total_dec_time = 0.0

    # print(f"Decoding Starting for {total} files with batch size = {batch_size}.")
    for i in range(0, total, batch_size):
        current_batch = files_batch_full_path_list[i:i + batch_size]
        start_time = datetime.datetime.now()
        img_list = decoder.decode(current_batch)
        end_time = datetime.datetime.now()
        time_per_frame = end_time - start_time
        total_dec_time = total_dec_time + time_per_frame.total_seconds()
        total_valid_images_processed += len(img_list)
        
        # calc mega pixels for all images
        if(len(img_list) > 0):
            for i, img in enumerate(img_list):
                mps.value += (float(img.width) * float(img.height)) / 1000000.0

    images_total.value = total_valid_images_processed
    bad_images.value = total-total_valid_images_processed

    if (total_valid_images_processed > 0 and total_dec_time > 0):
        time_per_frame = (total_dec_time / total_valid_images_processed) * 1000
        frame_per_second = total_valid_images_processed / total_dec_time
        ips.value = frame_per_second


if __name__ == "__main__":

    # get passed arguments
    parser = argparse.ArgumentParser(
        description='Batch decode example Arguments')
    parser.add_argument(
        '-i',
        '--input',
        type=str,
        help='Input Files FULL Path - required',
        required=True)
    parser.add_argument(
        '-b',
        '--batch',
        type=int,
        default=2,
        help='batch size > 0 process the batch of files with this batch size, if 0 means do not process as batch, optional, default is 2',
        required=False)
    parser.add_argument(
        '-fmt',
        '--output_format',
        type=int,
        choices=[3, 4],
        default=3,
        help='Set output image format: 3 for ROCJPEG_OUTPUT_RGB (interleaved), 4 for ROCJPEG_OUTPUT_RGB_PLANAR. Optional, default is 3.',
        required=False)
    parser.add_argument(
        '-bk',
        '--backend',
        type=int,
        choices=[0, 1],
        default=0,
        help='Set backend choice 0:GPU and 1:CPU, Optional, default is 0',
        required=False)    
    parser.add_argument(
        '-d',
        '--device',
        type=int,
        default=0,
        help='GPU device ID - optional, default 0',
        required=False)
    parser.add_argument(
        '-t',
        '--num_process',
        type=int,
        default=4,
        help='Num of parallel runs - optional, default 4',
        required=False)
    
    try:
        args = parser.parse_args()
    except BaseException:
        sys.exit()

    # get params
    input_file_path = args.input
    batch_size = args.batch
    output_format = args.output_format
    device_id = args.device
    backend = args.backend
    num_process = args.num_process

    # parse/process params
    if not isinstance(batch_size, int) or batch_size <= 0:
        print(f"Args Error: batch_size must be a positive integer, got {batch_size}\n")
        exit()
    if not os.path.isdir(input_file_path):
        print(f"Args Error: '{input_file_path}' is not a directory.\n")
        exit()
    if not os.path.exists(input_file_path):  # Input file or folder (must exist)
        print("ERROR: input folder doesn't exist.")
        exit()

    # prepare data collecting containers
    total_ips = 0.0
    total_mps = 0.0
    total_images = 0
    total_bad_images = 0

    # processes init
    try:
        multiprocessing.set_start_method('spawn')
    except RuntimeError:
        print('ERROR: Could not create processes')
        exit()

    # prepare shared containers
    processes = []
    mps = Value('f', 0.0)
    ips = Value('f', 0.0)
    images_total = Value('i', 0)
    bad_images = Value('i', 0)

    # Distribute files into num_process lists as equally as possible
    all_files_full_path = [os.path.join(root, f) for root, _, files in os.walk(input_file_path) for f in files]
    files_batch_full_path_list = [[] for _ in range(num_process)]
    for i, filepath in enumerate(all_files_full_path):
        files_batch_full_path_list[i % num_process].append(filepath)

    # create count of processes required by args.num_process
    for i in range(0, num_process):
        p = Process(target=jpeg_decode_batch_process, args=(files_batch_full_path_list[i], batch_size, output_format, device_id, backend, images_total, bad_images, mps, ips))
        p.start()
        processes.append(p)

    # launch the processes and synchronize collecting its data
    for p in processes:
        p.join()
        total_bad_images += bad_images.value
        total_images += images_total.value
        total_mps += mps.value
        total_ips += ips.value

    # printout results
    print("\ninfo: Total images decoded: " + str(total_images))
    print("info: Image per second: " + str(round(total_ips/total_images, 2)))
    print("info: Mega Pixel per second: " + str(round(total_mps/total_images, 2)))
    print("info: Total Bad files found : " + str(total_bad_images) + "\n")