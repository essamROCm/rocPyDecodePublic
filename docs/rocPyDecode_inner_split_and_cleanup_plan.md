# rocPyDecode inner split and cleanup

## Objective

Restructure rocPyDecode as a lightweight umbrella containing two independently configurable, buildable, installable, and testable CMake projects:

```text
rocPyDecode/
├── CMakeLists.txt
├── videoDecode/
└── jpegDecode/
```

The root project must build and test either component alone or both together. Each child must be self-contained so it can later move to a separate repository without depending on files in its sibling or the umbrella root.

## Approved scope

- `videoDecode` owns the rocDecode binding, `pyRocVideoDecode`, video samples, video tests, and its CMake discovery modules.
- `jpegDecode` owns the rocJPEG binding, `pyRocJpegDecode`, JPEG samples, JPEG tests, and its CMake discovery modules.
- Each child owns its component-specific documentation under a local `docs/`
  directory. The umbrella `docs/` directory owns only shared project guidance
  and provides the unified entry point to both component documentation trees.
- Each child owns a private copy of the buffer and DLPack implementation directly under its `src/` directory. There is no child `src/common/` directory.
- Types implemented independently by both native extensions are registered as
  module-local pybind11 types so both extensions can be imported in one Python
  process without duplicate global-registration errors.
- The root `CMakeLists.txt` is an orchestrator only.
- Native Python extension modules and Python package directories remain the delivered artifacts.
- Wheel creation, `setup.py`, `pyproject.toml`, and wheel validation are explicitly outside this PR.
- The root `data/` and `conda-recipe/` directories are removed.
- `build_rocpydecode_wheel.py` and `rocPyDecode-requirements.py` are removed.
- Other obsolete packaging/install helpers are removed only after confirming that they depend on the retired wheel or requirements workflows.
- The ResNet50 video sample accepts an optional labels file and remains usable without labels.
- Changed Python files will be checked with the repository formatting workflow where available; generated artifacts will not be committed.
- Because this restructuring changes the public source layout and build
  interface, the project version advances to the next major version, `1.0.0`.
  The root project, both child CMake projects, native-module version attributes,
  and changelog must remain synchronized.

## Target layout

```text
rocPyDecode/
├── CMakeLists.txt
├── README.md
├── CHANGELOG.md
├── LICENSE.txt
├── docs/
├── videoDecode/
│   ├── CMakeLists.txt
│   ├── README.md
│   ├── docs/
│   ├── cmake/
│   ├── pyRocVideoDecode/
│   ├── samples/rocdecode/
│   ├── src/
│   │   ├── roc_pybuffer.*
│   │   ├── roc_pydlpack.*
│   │   └── rocdecode/
│   └── tests/
└── jpegDecode/
    ├── CMakeLists.txt
    ├── README.md
    ├── docs/
    ├── cmake/
    ├── pyRocJpegDecode/
    ├── samples/rocjpeg/
    ├── src/
    │   ├── roc_pybuffer.*
    │   ├── roc_pydlpack.*
    │   └── rocjpeg/
    └── tests/
```

## CMake design

The root exposes `BUILD_VIDEO_DECODE` and `BUILD_JPEG_DECODE`, both enabled by default, and delegates with `add_subdirectory`. Each child owns component-prefixed dependency state, targets, paths, install rules, and tests so combined configuration has no collisions.

Each child supports:

```bash
cmake -S <child> -B <build> -DPYTHON_VERSION_SUGGESTED=<version>
cmake --build <build> --parallel
cmake --install <build>
ctest --test-dir <build> --output-on-failure
```

The umbrella supports the same sequence from the repository root and can disable either child independently.

## Test ownership

Video tests move to `videoDecode/tests`. The default CTest suite covers binding
types and direct raw H.264/H.265 GPU decoding. Tests and samples requiring
FFmpeg, rocDecode host support, Torch, hip-python, or VAAPI behavior remain
available for explicit execution but are not part of the dependency-minimal
default suite.

JPEG tests move to `jpegDecode/tests`. The dependency-minimal default CTest
uses batched JPEG decoding. The Torch-based single-image sample remains
available for explicit execution after Torch is installed.

CTest must consume build-tree outputs without requiring a prior install. Installed artifacts are validated separately through import checks.

## Cleanup and documentation

- Remove all references to deleted Conda, wheel, requirements-script, and old root-relative paths.
- Keep shared and umbrella documentation at the root. Move video- and
  JPEG-specific pages into each child's `docs/` tree and link to those trees
  from the root documentation.
- Give each child a local documentation index plus standalone build, install,
  test, dependency, API, and sample guidance. Keep concise quick-start and
  local-documentation links in each child README.
- Avoid duplicating component content in the root. This keeps documentation
  colocated with the implementation and minimizes work if either component is
  later extracted into a separate repository.
- Update architecture documentation so ownership boundaries and duplicated private support code are explicit.
- Record an ownership/movement map and the final validation evidence.

## Validation matrix

1. Configure, build, install, import, and CTest `videoDecode` standalone.
2. Configure, build, install, import, and CTest `jpegDecode` standalone.
3. Configure, build, and install both projects from the root.
4. Run combined root CTest and verify both components are represented.
5. Configure and build root with video only.
6. Configure and build root with JPEG only.
7. Verify the source tree contains no generated media links or build artifacts.
8. Run formatting and static repository checks applicable to changed files.

## Execution sequence

1. Start from current upstream `develop` on a dedicated branch.
2. Inventory and classify every root file and component dependency.
3. Create the independent video project and its test suite.
4. Create the independent JPEG project and its test suite.
5. Replace the monolithic root build with the umbrella orchestrator.
6. Remove approved obsolete files and repair affected samples/docs.
7. Validate standalone, component-only umbrella, and combined configurations on the target ROCm system.
8. Update this plan to match any justified implementation adjustments.
9. Produce the final movement, deletion, dependency, and validation report for the PR.

## Completion criteria

The work is complete only when both child projects independently configure, build, install, import, and pass their applicable CTests; the root does the same for both together; obsolete paths and workflows are absent; and the final documentation matches the tested implementation.

## Implementation adjustments and validation

The implementation retained the planned ownership boundaries. It uses the
active Python interpreter from the selected environment rather than probing
and building several Python versions in one configuration. This matches the
independent-project model and avoids cross-version target/path coupling.

FFmpeg-dependent binding sources are enabled when their dependencies are
available. The default CTest suite intentionally avoids optional FFmpeg,
rocDecode host, Torch, hip-python, and VAAPI-sensitive sample dependencies.

Validation on September 2, 2026 used Python 3.12, GPU target ``gfx942``, and a
ROCm 10.1 nightly SDK environment. Results:

- Standalone video: build and install passed; installed imports passed; 3/3
  applicable CTests passed.
- Standalone JPEG: build and install passed; installed imports passed; 1/1
  applicable CTest passed.
- Root video-only: build and install passed; 3/3 CTests passed.
- Root JPEG-only: build and install passed; 1/1 CTest passed.
- Root combined: build and install passed; 4/4 CTests passed.
- Both installed native modules and both Python packages imported successfully
  from the combined installation.
- Component-specific documentation is colocated under `videoDecode/docs` and
  `jpegDecode/docs`; the umbrella documentation links to both component trees.

Wheel creation was not performed and no wheel metadata was added, as approved
for this PR.
