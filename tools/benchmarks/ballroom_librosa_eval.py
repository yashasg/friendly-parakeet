#!/usr/bin/env python3
"""Evaluate librosa beat-tracking configs against Ballroom beat annotations."""

from __future__ import annotations

import argparse
import datetime as dt
import json
from pathlib import Path

import librosa
import numpy as np


def load_beats(path: Path) -> list[float]:
    beats: list[float] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                value = float(line.split()[0])
            except (IndexError, ValueError):
                continue
            if value >= 0.0:
                beats.append(value)
    return beats


def trim_edges(times: list[float], edge_seconds: float) -> list[float]:
    if len(times) < 3:
        return times
    end = times[-1] - edge_seconds
    if end <= edge_seconds:
        return times
    return [t for t in times if edge_seconds <= t <= end]


def discover_tracks(dataset_root: Path) -> list[tuple[Path, Path]]:
    audio_root = dataset_root / "audio"
    beats_root = dataset_root / "annotations" / "beats"
    tracks: list[tuple[Path, Path]] = []
    for audio in sorted(audio_root.glob("**/*.wav")):
        beats = beats_root / f"{audio.stem}.beats"
        if beats.exists():
            tracks.append((audio, beats))
    return tracks


def spectral_flux_onset_envelope(y: np.ndarray, hop_length: int, n_fft: int) -> np.ndarray:
    mag = np.abs(librosa.stft(y=y, n_fft=n_fft, hop_length=hop_length))
    flux = np.diff(mag, axis=1)
    flux = np.maximum(flux, 0.0)
    if flux.shape[1] == 0:
        return np.zeros((1,), dtype=np.float32)
    onset_env = flux.mean(axis=0)
    return np.concatenate([np.zeros((1,), dtype=onset_env.dtype), onset_env], axis=0)


def build_onset_envelope(y: np.ndarray, sr: int, cfg: dict) -> np.ndarray:
    hop_length = int(cfg["hop_length"])
    onset_kind = str(cfg.get("onset_envelope", "librosa_default"))
    if onset_kind == "stft_flux":
        n_fft = int(cfg.get("n_fft", 2048))
        return spectral_flux_onset_envelope(y=y, hop_length=hop_length, n_fft=n_fft)
    return librosa.onset.onset_strength(y=y, sr=sr, hop_length=hop_length)


def tempo_from_tempogram(onset_envelope: np.ndarray, sr: int, hop_length: int) -> np.ndarray:
    return librosa.feature.tempo(
        onset_envelope=onset_envelope,
        sr=sr,
        hop_length=hop_length,
        aggregate=None,
    )


def extract_beats(y: np.ndarray, sr: int, cfg: dict) -> list[float]:
    hop_length = int(cfg["hop_length"])
    tightness = float(cfg.get("tightness", 100.0))
    trim = bool(cfg.get("trim", True))
    onset_envelope = build_onset_envelope(y=y, sr=sr, cfg=cfg)
    tempo_source = str(cfg.get("tempo_source", "fixed_start_bpm"))

    kwargs: dict = {
        "onset_envelope": onset_envelope,
        "sr": sr,
        "hop_length": hop_length,
        "tightness": tightness,
        "trim": trim,
    }

    if tempo_source == "fixed_start_bpm":
        if "start_bpm" in cfg:
            kwargs["start_bpm"] = float(cfg["start_bpm"])
    else:
        tempo_series = tempo_from_tempogram(
            onset_envelope=onset_envelope,
            sr=sr,
            hop_length=hop_length,
        )
        if tempo_source == "tempogram_dynamic":
            kwargs["bpm"] = tempo_series
        else:
            kwargs["start_bpm"] = float(np.median(tempo_series))

    try:
        _, beat_frames = librosa.beat.beat_track(**kwargs)
    except Exception:
        if "bpm" in kwargs:
            tempo_series = kwargs.pop("bpm")
            kwargs["start_bpm"] = float(np.median(np.asarray(tempo_series)))
            _, beat_frames = librosa.beat.beat_track(**kwargs)
        else:
            raise
    beat_times = librosa.frames_to_time(beat_frames, sr=sr, hop_length=hop_length)
    return [float(v) for v in beat_times]


def match_counts(pred: list[float], truth: list[float], tol_s: float) -> tuple[int, int, int]:
    i = 0
    j = 0
    matched = 0
    while i < len(pred) and j < len(truth):
        delta = pred[i] - truth[j]
        if abs(delta) <= tol_s:
            matched += 1
            i += 1
            j += 1
        elif pred[i] < truth[j] - tol_s:
            i += 1
        else:
            j += 1
    fp = len(pred) - matched
    fn = len(truth) - matched
    return matched, fp, fn


def finalize(tp: int, fp: int, fn: int) -> dict[str, float]:
    precision = tp / (tp + fp) if (tp + fp) else 0.0
    recall = tp / (tp + fn) if (tp + fn) else 0.0
    f1 = (2.0 * precision * recall / (precision + recall)) if (precision + recall) else 0.0
    return {"precision": precision, "recall": recall, "f1": f1}


def evaluate_config(
    tracks: list[tuple[Path, Path]],
    cfg: dict,
    tolerances_ms: list[int],
    edge_trim_seconds: float,
) -> dict:
    totals: dict[int, dict[str, int]] = {ms: {"tp": 0, "fp": 0, "fn": 0} for ms in tolerances_ms}
    for audio_path, beats_path in tracks:
        y, sr = librosa.load(audio_path, sr=None, mono=True)
        truth = trim_edges(load_beats(beats_path), edge_seconds=edge_trim_seconds)
        pred = trim_edges(extract_beats(y=y, sr=sr, cfg=cfg), edge_seconds=edge_trim_seconds)
        for ms in tolerances_ms:
            tp, fp, fn = match_counts(pred, truth, tol_s=ms / 1000.0)
            totals[ms]["tp"] += tp
            totals[ms]["fp"] += fp
            totals[ms]["fn"] += fn

    out = {"config": cfg}
    for ms in tolerances_ms:
        metrics = finalize(
            tp=totals[ms]["tp"],
            fp=totals[ms]["fp"],
            fn=totals[ms]["fn"],
        )
        out[f"f1_{ms}"] = metrics["f1"]
        out[f"rec_{ms}"] = metrics["recall"]
        out[f"prec_{ms}"] = metrics["precision"]
    out["gate"] = (out["f1_20"] + out["f1_35"]) / 2.0
    return out


def main() -> None:
    parser = argparse.ArgumentParser(description="Evaluate librosa beat configs on Ballroom")
    parser.add_argument("--dataset-root", default="benchmarks/ballroom-local")
    parser.add_argument(
        "--config",
        default="benchmarks/ballroom-eval/librosa_fine_tuning_config.json",
    )
    parser.add_argument("--output-root", default="benchmarks/ballroom-eval/results")
    args = parser.parse_args()

    dataset_root = Path(args.dataset_root)
    config_path = Path(args.config)
    output_root = Path(args.output_root)
    output_root.mkdir(parents=True, exist_ok=True)

    cfg = json.loads(config_path.read_text(encoding="utf-8"))
    tolerances_ms = [int(v) for v in cfg["tolerances_ms"]]
    edge_trim_seconds = float(cfg.get("edge_trim_seconds", 0.25))
    configs = list(cfg["configs"])

    tracks = discover_tracks(dataset_root)
    if not tracks:
        raise SystemExit(f"No tracks found under {dataset_root}/audio with matching .beats files")

    results: list[dict] = []
    for index, config_item in enumerate(configs, start=1):
        print(f"[{index}/{len(configs)}] {config_item.get('name', 'unnamed-config')}", flush=True)
        row = evaluate_config(
            tracks=tracks,
            cfg=config_item,
            tolerances_ms=tolerances_ms,
            edge_trim_seconds=edge_trim_seconds,
        )
        results.append(row)

    ranked = sorted(results, key=lambda r: r["gate"], reverse=True)
    run_id = "librosa-finetune-op-" + dt.datetime.now(dt.UTC).strftime("%Y%m%dT%H%M%SZ")
    payload = {
        "run_id": run_id,
        "tracks_total": len(tracks),
        "dataset_root": str(dataset_root),
        "config_path": str(config_path),
        "ranked": ranked,
        "best": ranked[0] if ranked else None,
    }
    out_path = output_root / f"{run_id}.json"
    out_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    print(f"Output: {out_path}")
    if ranked:
        best = ranked[0]
        print(
            "Best:"
            f" {best['config'].get('name', 'unnamed')}"
            f" | rec@20={best['rec_20']:.4f}"
            f" rec@35={best['rec_35']:.4f}"
            f" rec@70={best['rec_70']:.4f}"
            f" | gate={best['gate']:.4f}"
        )


if __name__ == "__main__":
    main()
