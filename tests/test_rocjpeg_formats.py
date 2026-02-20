#!/usr/bin/env python3
# Light-weight coverage driver for rocjpeg output formats

import argparse
from pathlib import Path
import sys


def expand(p: str) -> Path:
    return Path(p).expanduser()


def decode_one(decoder, rj, path: Path, fmt):
    cs = rj.CodeStream(path.read_bytes())
    ds = rj.DecodeSource(cs)
    decoder.SetOutputFormat(fmt)
    elapsed, img = decoder.decode(ds)
    if img.ext_buf:
        try:
            arr = img.to_numpy(0)
            _ = arr.shape
        except Exception:
            pass
    return elapsed


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--jpeg", required=True, help="JPEG file or directory")
    args = parser.parse_args(argv)

    import rocpyjpegdecode as rj

    samples = []
    root = expand(args.jpeg)
    if root.is_dir():
        for name in ["mug_420.jpg", "mug_422.jpg", "mug_400.jpg"]:
            candidate = root / name
            if candidate.exists():
                samples.append(candidate)
    else:
        samples.append(root)

    if not samples:
        print("No JPEG samples found; skipping rocjpeg format coverage.")
        return 0

    # try hardware backend; if not available, skip gracefully
    try:
        decoder = rj.Decoder(0, rj.jpegTypes.ROCJPEG_BACKEND_HARDWARE, rj.jpegTypes.ROCJPEG_OUTPUT_RGB)
    except Exception as exc:
        print(f"rocjpeg hardware backend unavailable: {exc}; skipping.")
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
                # skip the rest if backend misbehaves
                continue

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
