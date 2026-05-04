#!/usr/bin/env python3
"""Prepare local Ballroom subset for librosa benchmarking."""

from __future__ import annotations

import argparse
import io
import json
import tarfile
import urllib.request
import zipfile
from pathlib import Path

BEATS_URL = "https://github.com/CPJKU/BallroomAnnotations/archive/refs/heads/master.zip"
TEMPO_URL = "https://mtg.upf.edu/ismir2004/contest/tempoContest/data2.tar.gz"
AUDIO_URL = "https://mtg.upf.edu/ismir2004/contest/tempoContest/data1.tar.gz"


def download(url: str, out_path: Path) -> None:
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url) as resp, out_path.open("wb") as f:
        f.write(resp.read())


def extract_beats(zip_path: Path, beats_dir: Path) -> int:
    beats_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    with zipfile.ZipFile(zip_path, "r") as zf:
        for name in zf.namelist():
            if not name.endswith(".beats"):
                continue
            target = beats_dir / Path(name).name
            with zf.open(name) as src, target.open("wb") as dst:
                dst.write(src.read())
            count += 1
    return count


def extract_tempo(tar_path: Path, tempo_dir: Path) -> int:
    tempo_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    with tarfile.open(tar_path, "r:gz") as tf:
        for member in tf.getmembers():
            if not member.isfile() or not member.name.endswith(".bpm"):
                continue
            src = tf.extractfile(member)
            if src is None:
                continue
            target = tempo_dir / Path(member.name).name
            with src, target.open("wb") as dst:
                dst.write(src.read())
            count += 1
    return count


def extract_audio_subset(archive_path: Path, audio_dir: Path, genres: list[str], max_tracks: int) -> list[dict]:
    audio_dir.mkdir(parents=True, exist_ok=True)
    genre_set = {g.lower() for g in genres}
    selected: list[dict] = []
    with tarfile.open(archive_path, "r:gz") as tf:
        for member in tf:
            if len(selected) >= max_tracks:
                break
            if not member.isfile() or not member.name.endswith(".wav"):
                continue
            parts = Path(member.name).parts
            if len(parts) < 3:
                continue
            genre = parts[1]
            if genre.lower() not in genre_set:
                continue
            src = tf.extractfile(member)
            if src is None:
                continue
            target = audio_dir / genre / Path(member.name).name
            target.parent.mkdir(parents=True, exist_ok=True)
            with src, target.open("wb") as dst:
                dst.write(src.read())
            selected.append({"track_id": target.stem, "genre": genre, "audio_path": str(target)})
    return selected


def main() -> None:
    parser = argparse.ArgumentParser(description="Setup local Ballroom subset for librosa benchmark")
    parser.add_argument("--dataset-root", default="benchmarks/ballroom-local", help="Local Ballroom dataset root")
    parser.add_argument("--download-audio", action="store_true", help="Download 1.45GB audio archive")
    parser.add_argument("--audio-archive", default=None, help="Existing data1.tar.gz path (optional)")
    parser.add_argument("--max-tracks", type=int, default=8, help="How many audio tracks to extract")
    parser.add_argument(
        "--genres",
        nargs="+",
        default=["Jive", "Quickstep", "Waltz"],
        help="Genres to include when extracting subset",
    )
    args = parser.parse_args()

    root = Path(args.dataset_root)
    archives = root / "archives"
    beats_dir = root / "annotations" / "beats"
    tempo_dir = root / "annotations" / "tempo"
    audio_dir = root / "audio"

    beats_zip = archives / "ballroom_beats.zip"
    tempo_tar = archives / "ballroom_tempo.tar.gz"

    if not beats_zip.exists():
        print(f"Downloading beat annotations -> {beats_zip}")
        download(BEATS_URL, beats_zip)
    if not tempo_tar.exists():
        print(f"Downloading tempo annotations -> {tempo_tar}")
        download(TEMPO_URL, tempo_tar)

    beats_count = extract_beats(beats_zip, beats_dir)
    tempo_count = extract_tempo(tempo_tar, tempo_dir)

    selected: list[dict] = []
    audio_archive = Path(args.audio_archive) if args.audio_archive else archives / "ballroom_audio.tar.gz"
    if args.download_audio and not audio_archive.exists():
        print(f"Downloading audio archive (large) -> {audio_archive}")
        download(AUDIO_URL, audio_archive)

    if audio_archive.exists():
        print(f"Extracting audio subset from {audio_archive}")
        selected = extract_audio_subset(audio_archive, audio_dir, args.genres, args.max_tracks)
    else:
        print("Audio archive missing; skipping audio extraction")

    manifest = {
        "dataset_root": str(root),
        "beats_count": beats_count,
        "tempo_count": tempo_count,
        "selected_tracks": selected,
        "audio_archive": str(audio_archive) if audio_archive.exists() else None,
    }
    manifest_path = root / "manifest.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    with manifest_path.open("w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    print(f"Wrote manifest: {manifest_path}")
    print(f"Selected tracks: {len(selected)}")


if __name__ == "__main__":
    main()
