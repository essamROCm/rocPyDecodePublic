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

import rocpyjpegdecode as rocpyjpeg
import os
import shutil
import argparse
from datetime import datetime
import csv

def copy_images(base_dir, num_images, benchmark_mode):
    resolutions = ['1920_1080', '3840_2160'] if benchmark_mode == 0 else ['640_480', '800_600', '1920_1080', '3840_2160', '7680_4320', '16384_16384'] # '3991_2661'
    for category in ['flowers']:
        for resolution in resolutions:
            dir_path = os.path.join(base_dir, category, resolution)
            images = [f for f in os.listdir(dir_path) if f.endswith('.jpg')]
            if len(images) < num_images:
                image_path = os.path.join(dir_path, images[0])
                print(f"Copying images in {dir_path}")
                for i in range(len(images), num_images):
                    new_image_path = os.path.join(dir_path, f'{os.path.splitext(images[0])[0]}_{i}.jpg')
                    shutil.copy(image_path, new_image_path)

def run_benchmark(base_dir, batch_size, num_threads, num_images, device_id, benchmark_mode):
    results = []
    resolutions = (['1920_1080', '3840_2160'] if benchmark_mode == 0 else ['640_480', '800_600', '1920_1080', '3840_2160', '7680_4320', '16384_16384'])
    for category in ['flowers']:
        for resolution in resolutions:
            dir_path = os.path.join(base_dir, category, resolution)
            best_ips, best_mps = 0, 0
            for _ in range(3):
                print(f"decoding images in {dir_path}")
                _, ips, mps = rocpyjpeg.decode_with_perfromance(dir_path, batch_size, num_threads, device_id)                
                if ips > best_ips:
                    best_ips, best_mps = ips, mps
            results.append([category, 'Native', resolution.replace('_', 'x'), num_images, best_ips, best_mps])   
    return results

def save_results(results, batch_size, num_threads, device_id):
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    csv_filename = f'benchmark_results_{timestamp}_batch_size_{batch_size}_num_threads_{num_threads}_device_id_{device_id}.csv'
    with open(csv_filename, 'w', newline='') as csvfile:
        fieldnames = ['Image Category', 'Decode Type', 'Resolution', 'Number of Images', 'Images Per Seconds (IPS)', 'MPixels/sec']
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        writer.writeheader()
        for result in results:
            writer.writerow({
                'Image Category': result[0],
                'Decode Type': result[1],
                'Resolution': result[2],
                'Number of Images': result[3],
                'Images Per Seconds (IPS)': f"{result[4]:.3f}",
                'MPixels/sec': f"{result[5]:.3f}"
            })

def main():
    parser = argparse.ArgumentParser(description='Benchmark JPEG decoding using AMD ROCm rocJPEG APIs.')
    parser.add_argument('-i','--input', type=str, help='Input File-FULL-Path - required', required=True)
    parser.add_argument('-n', '--num_images', type=int, default=100, help='Number of images to copy (default: 1440)')
    parser.add_argument('-b', '--batch_size', type=int, default=24, help='Batch size for benchmarking (default: 24)')
    parser.add_argument('-t', '--num_threads', type=int, default=1, help='Number of threads for benchmarking (default: 1)')
    parser.add_argument('-d', '--device_id', type=int, default=0, help='Device id for the GPU for benchmarking (default: 0)')
    parser.add_argument('-m', '--benchmark_mode', type=int, default=0, help='Benchmark mode: 0 for light, 1 for full (default: 0)')
    args = parser.parse_args()

    copy_images(args.input, args.num_images, args.benchmark_mode)
    results = run_benchmark(args.input, args.batch_size, args.num_threads, args.num_images, args.device_id, args.benchmark_mode)
    save_results(results, args.batch_size, args.num_threads, args.device_id)
    print("Benchmarking completed!")

if __name__ == '__main__':
    main()
