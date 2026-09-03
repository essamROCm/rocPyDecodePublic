# Component ownership map

| Previous location | New owner | New location or disposition |
|---|---|---|
| `src/rocdecode` | Video | `videoDecode/src/rocdecode` |
| `pyRocVideoDecode` | Video | `videoDecode/pyRocVideoDecode` |
| `samples/rocdecode` | Video | `videoDecode/samples/rocdecode` |
| Python video tests | Video | `videoDecode/tests` |
| `FindFFmpeg.cmake` | Video | `videoDecode/cmake` |
| `src/rocjpeg` | JPEG | `jpegDecode/src/rocjpeg` |
| `pyRocJpegDecode` | JPEG | `jpegDecode/pyRocJpegDecode` |
| `samples/rocjpeg` | JPEG | `jpegDecode/samples/rocjpeg` |
| JPEG sample CTests | JPEG | `jpegDecode/tests` |
| `src/common/*` | Private to each child | Duplicated directly under each child `src` |
| `FindDLPACK.cmake` | Private to each child | Duplicated under each child `cmake` |
| Video-specific documentation | Video | `videoDecode/docs` |
| JPEG-specific documentation | JPEG | `jpegDecode/docs` |
| Shared documentation and publication entry point | Umbrella | `docs` |
| Root `CMakeLists.txt` | Umbrella | Reduced to component orchestration |
| `data`, `conda-recipe` | Obsolete | Removed |
| Wheel/setup/requirements helpers | Out of scope or obsolete | Removed; native modules remain CMake-installed artifacts |

The duplicated support sources are intentionally owned by their respective
components. Neither child includes or builds files from its sibling or from an
umbrella source directory.

The same ownership rule applies to documentation. Component API, prerequisite,
installation, and sample material is colocated with its child. The umbrella
documentation describes the combined project and links to both child
documentation indexes.
