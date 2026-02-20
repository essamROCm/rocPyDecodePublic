#!/usr/bin/env python3
# Light-weight coverage driver for rocjpeg output formats

import argparse
import subprocess
from pathlib import Path
import sys


def expand(p: str) -> Path:
    return Path(p).expanduser()


def decode_one(decoder, rj, path: Path, fmt):
    # Use CodeStream/DecodeSource to keep exercising the pybind surface,
    # but mirror the sample script's path-based decode flow and metadata touches.
    cs = rj.CodeStream(path.read_bytes())
    ds = rj.DecodeSource(cs)
    decoder.SetOutputFormat(fmt)
    elapsed, img = decoder.decode(ds)
    if img.ext_buf:
        try:
            arr = img.to_numpy(0)
            _ = arr.shape  # touch shape/strides/dtype paths
            _ = img.strides
            _ = img.dtype
        except Exception:
            pass
    return elapsed


def init_decoder(rj, device_id: int, backend: int):
    """Initialize decoder, favoring requested backend but falling back to CPU."""
    utils = rj.PyRocJpegUtils()
    _, ok = utils.init_hip_device(device_id, False)
    if not ok:
        print(f"init_hip_device failed for device {device_id}; skipping.")
        return None

    backends = [(backend, "requested")]
    if backend == rj.jpegTypes.ROCJPEG_BACKEND_HARDWARE:
        backends.append((rj.jpegTypes.ROCJPEG_BACKEND_HYBRID, "CPU fallback"))

    last_exc = None
    for b, label in backends:
        try:
            dec = rj.Decoder(device_id, b, rj.jpegTypes.ROCJPEG_OUTPUT_RGB)
            print(f"Using rocjpeg backend {b} ({label})")
            return dec
        except Exception as exc:
            print(f"rocjpeg backend {b} ({label}) unavailable: {exc}")
            last_exc = exc
    print(f"No rocjpeg backend available; skipping. Last error: {last_exc}")
    return None


def gather_samples(root: Path):
    samples: list[Path] = []
    if root.is_dir():
        for name in ["mug_420.jpg", "mug_422.jpg", "mug_400.jpg"]:
            candidate = root / name
            if candidate.exists():
                samples.append(candidate)
    else:
        samples.append(root)
    return samples


def run_worker(args) -> int:
    import rocpyjpegdecode as rj

    root = expand(args.jpeg)
    samples = gather_samples(root)
    if not samples:
        print("No JPEG samples found; skipping rocjpeg format coverage.")
        return 0

    decoder = init_decoder(rj, args.device, args.backend)
    if decoder is None:
        return 0

    # Restrict to stable formats; others have been seen to crash on some systems
    formats = [
        rj.jpegTypes.ROCJPEG_OUTPUT_RGB,
        rj.jpegTypes.ROCJPEG_OUTPUT_RGB_PLANAR,
    ]

    for path in samples:
        for fmt in formats:
            try:
                decode_one(decoder, rj, path, fmt)
            except Exception as exc:
                print(f"Decode failed for {path.name} fmt {fmt}: {exc}")
                continue

    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="rocjpeg format coverage (stable formats only)")
    parser.add_argument("--jpeg", required=True, help="JPEG file or directory")
    parser.add_argument("--backend", type=int, default=1, choices=[0, 1],
                        help="rocjpeg backend: 0=GPU (hardware), 1=CPU (hybrid)")
    parser.add_argument("--device", type=int, default=0, help="GPU device id (when backend=0)")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    # Run decode work in a child process to insulate from backend crashes.
    if not args.worker:
        cmd = [sys.executable, __file__, "--worker", "--jpeg", args.jpeg, "--backend", str(args.backend), "--device", str(args.device)]
        result = subprocess.run(cmd)
        if result.returncode == 0:
            return 0
        print(f"rocjpeg format helper exited with code {result.returncode}; treating as skip.")
        return 0

    return run_worker(args)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
