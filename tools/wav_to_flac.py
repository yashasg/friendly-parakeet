#!/usr/bin/env python3
"""Convert all .wav files in content/audio/ to FLAC and remove the originals.

Usage:
    python3 tools/wav_to_flac.py              # convert + delete originals
    python3 tools/wav_to_flac.py --keep-wav   # convert but keep originals
    python3 tools/wav_to_flac.py --dry-run    # preview only, no changes

Requires: ffmpeg on PATH.
"""

import argparse
import subprocess
import sys
from pathlib import Path

AUDIO_DIR = Path(__file__).resolve().parent.parent / "content" / "audio"


def convert(wav: Path, *, dry_run: bool = False) -> Path:
    flac = wav.with_suffix(".flac")
    print(f"  {wav.name} -> {flac.name}", end="")

    if dry_run:
        print("  [dry-run]")
        return flac

    result = subprocess.run(
        ["ffmpeg", "-y", "-i", str(wav), "-c:a", "flac", str(flac)],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("  FAILED")
        print(result.stderr, file=sys.stderr)
        sys.exit(1)

    wav_mb = wav.stat().st_size / (1024 * 1024)
    flac_mb = flac.stat().st_size / (1024 * 1024)
    ratio = (1 - flac_mb / wav_mb) * 100
    print(f"  {wav_mb:.1f} MB -> {flac_mb:.1f} MB  ({ratio:.0f}% smaller)")
    return flac


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert wav files to FLAC.")
    parser.add_argument("--keep-wav", action="store_true", help="Keep original .wav files")
    parser.add_argument("--dry-run", action="store_true", help="Preview without converting")
    args = parser.parse_args()

    if not AUDIO_DIR.is_dir():
        print(f"Audio directory not found: {AUDIO_DIR}", file=sys.stderr)
        sys.exit(1)

    wavs = sorted(AUDIO_DIR.glob("*.wav"))
    if not wavs:
        print("No .wav files found.")
        return

    print(f"Found {len(wavs)} wav file(s) in {AUDIO_DIR}\n")

    for wav in wavs:
        convert(wav, dry_run=args.dry_run)

    if not args.dry_run and not args.keep_wav:
        print()
        for wav in wavs:
            wav.unlink()
            print(f"  Deleted {wav.name}")

    print("\nDone.")


if __name__ == "__main__":
    main()
