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
import rocpyjpegdecode.jpegTypes as jpegt
import argparse
import os
import sys

# -----------------------------------------------------------------------
# decode all files under a folder and its sub-folders in one images list
# process the files in batch(es) according to the passed 'batch_size'
# -----------------------------------------------------------------------
def decode_batch(decoder, folder_full_path, batch_size):
    if not isinstance(batch_size, int) or batch_size <= 0:
        raise ValueError(f"batch_size must be a positive integer, got {batch_size}")        
    if not os.path.isdir(folder_full_path):
        raise NotADirectoryError(f"'{folder_full_path}' is not a valid directory.")       
    files_full_path_list = [os.path.join(root, f) for root, _, files in os.walk(folder_full_path) for f in files]
    total = len(files_full_path_list)
    total_valid_images_processed = 0
    print(f"Decoding Starting for {total} files with batch size = {batch_size}.")
    for i in range(0, total, batch_size):
        current_batch = files_full_path_list[i:i + batch_size]
        img_list = decoder.decode(current_batch)
        # consum this batch now, it will not be freed if new batch is decoded
        for img in img_list:
            if img.is_valid():
                total_valid_images_processed += 1
    print(f"Total files processed : {total_valid_images_processed}")
    print(f"Total Bad files found : {total-total_valid_images_processed}")

# -----------------------------------------------------------------------
# decode all files under a folder and its sub-folders in one images list
# process the files one by one not in a batch
# -----------------------------------------------------------------------

def decode_single(decoder, folder_full_path):
    if not os.path.isdir(folder_full_path):
        raise NotADirectoryError(f"'{folder_full_path}' is not a valid directory.")
    files_full_path_list = [os.path.join(root, f) for root, _, files in os.walk(folder_full_path) for f in files]
    total = len(files_full_path_list)
    total_valid_images_processed = 0
    for file_path in files_full_path_list:
        one_image = decoder.decode(file_path)
        # consum the image now before new one is decoded
        if one_image.is_valid():
            total_valid_images_processed += 1
    print(f"Total files processed : {total_valid_images_processed}")
    print(f"Total Bad files found : {total-total_valid_images_processed}")

# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
# Example of multiple images decoding process
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
def jpeg_decode(input_file_path, single, batch, output_format):

    device_id = 0   # example, change it to your system GPU index
    backend = 0     # 0 for GPU 1 for CPU

    # create the decode instance
    decoder = jdec.decoder(device_id, backend)
    decoder.set_output_image_format(output_format)  # set the output image to the desired format

    print(f"Image output format set to: {jpegt.RocJpegOutputFormat(output_format)}")

    line = '\n' + '-' * 60 + '\n'
    root_folder = input_file_path   # user input
    batch_size = batch              # user input

    if(single >= 1):
        # ---------------------------------------------------------------
        # TEST (1) Decode ALL files (ONE by ONE) under Folder/SubFolders
        # --------------------------------------------------------------
        print(line, "TEST (1) Decode ALL files under Folder/SubFolders ONE by ONE", line)
        decode_single(decoder, root_folder)
    else:
        print("No batch decoding with single files requested, you can pass -s 1 to perfrom it.")

    if(batch>=1):
        # ---------------------------------------------------------------
        # TEST (2) Decode ALL files (in batches) under Folder/SubFolders
        # --------------------------------------------------------------
        print(line, "TEST (2) Decode ALL files under Folder/SubFolders in batches", line)
        decode_batch(decoder, root_folder, batch_size)
    else:
        print("No batch decoding with batch size requested, you can pass -b x where 'x' is > 0.")


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
        '-s',
        '--single',
        type=int,
        default=0,
        help='single > 0 process batch of files one by one, single process, set to 0 mean no single files process.',
        required=False)
    parser.add_argument(
        '-b',
        '--batch',
        type=int,
        default=0,
        help='batch size > 0 process the batch of files with this batch size, if 0 means do not process as batch',
        required=False)
    parser.add_argument(
        '-fmt',
        '--output_format',
        type=int,
        choices=[3, 4],
        default=3,
        help='Set output image format: 3 for ROCJPEG_OUTPUT_RGB (interleaved), 4 for ROCJPEG_OUTPUT_RGB_PLANAR. Optional, default is 3.',
        required=False)

    try:
        args = parser.parse_args()
    except BaseException:
        sys.exit()

    # get params
    input_file_path = args.input
    single = args.single
    batch = args.batch
    output_format = args.output_format

    if not os.path.exists(input_file_path):  # Input file (must exist)
        print("ERROR: input file doesn't exist.")
        exit()

    jpeg_decode(input_file_path, single, batch, output_format)