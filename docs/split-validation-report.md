# rocPyDecode split validation report

Date: September 2, 2026

Validated project version: `1.0.0`

## Environment

- Ubuntu 24.04 container
- ROCm 10.1 nightly SDK snapshot `20260825-32791995050`
- Python 3.12
- GPU target `gfx942`
- One visible GPU used for runtime validation

## Results

| Configuration | Build | Install | CTest | Installed import |
|---|---:|---:|---:|---:|
| `videoDecode` standalone | Pass | Pass | 3/3 | Pass |
| `jpegDecode` standalone | Pass | Pass | 1/1 | Pass |
| Root, video only | Pass | Pass | 3/3 | Covered by standalone |
| Root, JPEG only | Pass | Pass | 1/1 | Covered by standalone |
| Root, combined | Pass | Pass | 4/4 | Both components pass |

The video test set contained the binding type test and H.264/H.265 raw
bitstream decode tests. The JPEG test set contained the batched JPEG decode
test. Samples requiring FFmpeg, the rocDecode host library, Torch, hip-python,
or VAAPI-sensitive behavior are available for explicit use but are excluded
from the dependency-minimal default CTest suite.

## Installed combined artifacts

The combined installation contained:

- `lib/rocpydecode.cpython-312-x86_64-linux-gnu.so`
- `lib/rocpyjpegdecode.cpython-312-x86_64-linux-gnu.so`
- `lib/pyRocVideoDecode/`
- `lib/pyRocJpegDecode/`

Each native extension and its associated Python package imported successfully
from the combined installation.
The video and JPEG extensions also imported together in one Python process
after their private `BufferInterface` bindings were made module-local.
