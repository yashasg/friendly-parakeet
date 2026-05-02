#!/usr/bin/env python3
"""Run aubio beat/tempo evaluation and tuning using Ballroom annotations."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import math
import subprocess
from pathlib import Path

TOLERANCES = [0.01, 0.02, 0.035, 0.05, 0.07]
GATE_TOLERANCES = [0.02, 0.035]
BASELINE = {"bufsize": 1024, "hopsize": 512}
BUF_CANDIDATES = [512, 1024, 2048, 4096]
HOP_CANDIDATES = [128, 256, 512, 1024]


def run_aubio(command: str, audio: Path, params: dict) -> str:
    cmd = [
        "aubio",
        command,
        str(audio),
        "-B",
        str(params["bufsize"]),
        "-H",
        str(params["hopsize"]),
    ]
    out = subprocess.run(cmd, capture_output=True, text=True, check=True)
    return out.stdout


def parse_times(raw: str) -> list[float]:
    vals: list[float] = []
    for line in raw.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            vals.append(float(line.split("\t")[0].split()[0]))
        except (ValueError, IndexError):
            continue
    return vals


def load_beats(path: Path) -> list[float]:
    beats: list[float] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                t = float(line.split()[0])
            except (ValueError, IndexError):
                continue
            if t >= 0.0:
                beats.append(t)
    return beats


def load_bpm(path: Path) -> float:
    with path.open("r", encoding="utf-8") as f:
        first = f.readline().strip().split(",")[0]
    return float(first)


def match_metrics(pred: list[float], truth: list[float], tol: float) -> dict:
    i = 0
    j = 0
    matched = 0
    diffs: list[float] = []
    while i < len(pred) and j < len(truth):
        delta = pred[i] - truth[j]
        if abs(delta) <= tol:
            matched += 1
            diffs.append(delta)
            i += 1
            j += 1
        elif pred[i] < truth[j] - tol:
            i += 1
        else:
            j += 1

    fp = len(pred) - matched
    fn = len(truth) - matched
    precision = matched / (matched + fp) if (matched + fp) else 0.0
    recall = matched / (matched + fn) if (matched + fn) else 0.0
    f1 = (2.0 * precision * recall / (precision + recall)) if (precision + recall) else 0.0
    mae = sum(abs(d) for d in diffs) / len(diffs) if diffs else None
    bias = sum(diffs) / len(diffs) if diffs else None
    return {
        "tolerance_s": tol,
        "tp": matched,
        "fp": fp,
        "fn": fn,
        "precision": precision,
        "recall": recall,
        "f1": f1,
        "mae_s": mae,
        "bias_s": bias,
    }


def evaluate_track(audio: Path, beats_path: Path, bpm_path: Path, params: dict) -> dict:
    truth_beats = load_beats(beats_path)
    truth_bpm = load_bpm(bpm_path)
    pred_beats = parse_times(run_aubio("beat", audio, params))

    tempo_raw = run_aubio("tempo", audio, params)
    pred_bpm = None
    for value in parse_times(tempo_raw):
        pred_bpm = value
        break

    metrics = [match_metrics(pred_beats, truth_beats, tol) for tol in TOLERANCES]
    bpm_error = None if pred_bpm is None else abs(pred_bpm - truth_bpm)

    return {
        "track_id": audio.stem,
        "genre": audio.parent.name,
        "audio": str(audio),
        "truth_beats": len(truth_beats),
        "pred_beats": len(pred_beats),
        "truth_bpm": truth_bpm,
        "pred_bpm": pred_bpm,
        "bpm_abs_error": bpm_error,
        "metrics": metrics,
    }


def aggregate(track_results: list[dict]) -> dict:
    summary: dict[str, dict] = {}
    for tol in TOLERANCES:
        key = f"f1_{int(tol * 1000)}ms"
        vals = [
            next(m["f1"] for m in tr["metrics"] if abs(m["tolerance_s"] - tol) < 1e-9)
            for tr in track_results
        ]
        summary[key] = {
            "macro_f1": (sum(vals) / len(vals)) if vals else 0.0,
            "min_f1": min(vals) if vals else 0.0,
            "max_f1": max(vals) if vals else 0.0,
        }

    bpm_errors = [tr["bpm_abs_error"] for tr in track_results if tr["bpm_abs_error"] is not None]
    summary["bpm"] = {
        "mae": (sum(bpm_errors) / len(bpm_errors)) if bpm_errors else None,
        "max_abs_error": max(bpm_errors) if bpm_errors else None,
    }
    return summary


def discover_tracks(dataset_root: Path) -> list[tuple[Path, Path, Path]]:
    beats_dir = dataset_root / "annotations" / "beats"
    tempo_dir = dataset_root / "annotations" / "tempo"
    tracks: list[tuple[Path, Path, Path]] = []
    for audio in sorted((dataset_root / "audio").glob("**/*.wav")):
        beats = beats_dir / f"{audio.stem}.beats"
        tempo = tempo_dir / f"{audio.stem}.bpm"
        if beats.exists() and tempo.exists():
            tracks.append((audio, beats, tempo))
    return tracks


def evaluate_config(tracks: list[tuple[Path, Path, Path]], params: dict) -> dict:
    track_results = [evaluate_track(audio, beats, tempo, params) for audio, beats, tempo in tracks]
    return {
        "params": params,
        "tracks_evaluated": len(track_results),
        "aggregate": aggregate(track_results),
        "tracks": track_results,
    }


def aggregate_f1(result: dict, tol: float) -> float:
    key = f"f1_{int(tol * 1000)}ms"
    return float(result["aggregate"][key]["macro_f1"])


def gate_score(result: dict) -> float:
    return sum(aggregate_f1(result, tol) for tol in GATE_TOLERANCES) / len(GATE_TOLERANCES)


def score_tuple(result: dict) -> tuple[float, float, float, float]:
    bpm_mae = result["aggregate"]["bpm"]["mae"]
    return (
        gate_score(result),
        aggregate_f1(result, 0.05),
        aggregate_f1(result, 0.07),
        -1.0 * (bpm_mae if bpm_mae is not None else math.inf),
    )


def append_progress(progress_path: Path, label: str, iteration: int, result: dict) -> None:
    agg = result["aggregate"]
    rows = {
        "f1_10ms": agg["f1_10ms"]["macro_f1"],
        "f1_20ms": agg["f1_20ms"]["macro_f1"],
        "f1_35ms": agg["f1_35ms"]["macro_f1"],
        "f1_50ms": agg["f1_50ms"]["macro_f1"],
        "f1_70ms": agg["f1_70ms"]["macro_f1"],
    }
    gate = gate_score(result)
    bpm_mae = agg["bpm"]["mae"]
    bpm_mae_text = "n/a" if bpm_mae is None else f"{bpm_mae:.4f}"
    params = result["params"]
    with progress_path.open("a", encoding="utf-8") as f:
        f.write(
            f"| {iteration} | {label} | {params['bufsize']} | {params['hopsize']} | "
            f"{rows['f1_10ms']:.4f} | {rows['f1_20ms']:.4f} | {rows['f1_35ms']:.4f} | "
            f"{rows['f1_50ms']:.4f} | {rows['f1_70ms']:.4f} | {gate:.4f} | {bpm_mae_text} |\n"
        )


def append_iteration_note(
    progress_path: Path,
    iteration: int,
    current: dict,
    best_neighbor: dict | None,
    improved: bool,
) -> None:
    current_gate = gate_score(current)
    with progress_path.open("a", encoding="utf-8") as f:
        if improved and best_neighbor is not None:
            candidate_gate = gate_score(best_neighbor)
            delta = candidate_gate - current_gate
            f.write(
                f"\n- Iteration {iteration}: improved gate from {current_gate:.4f} to {candidate_gate:.4f} "
                f"(Δ {delta:+.4f}) with params `{best_neighbor['params']}`.\n"
            )
        else:
            f.write(
                f"\n- Iteration {iteration}: no improving neighbor found around `{current['params']}`; stopping.\n"
            )


def normalize_params(params: dict) -> dict:
    return {"bufsize": int(params["bufsize"]), "hopsize": int(params["hopsize"])}


def params_key(params: dict) -> tuple[int, int]:
    p = normalize_params(params)
    return (p["bufsize"], p["hopsize"])


def valid_pair(bufsize: int, hopsize: int) -> bool:
    return bufsize in BUF_CANDIDATES and hopsize in HOP_CANDIDATES and hopsize <= bufsize


def neighbor_configs(current: dict) -> list[dict]:
    buf = int(current["bufsize"])
    hop = int(current["hopsize"])
    candidates = [
        (buf * 2, hop),
        (buf // 2, hop),
        (buf, hop * 2),
        (buf, hop // 2),
        (buf * 2, hop // 2),
        (buf // 2, hop * 2),
    ]
    out: list[dict] = []
    seen: set[tuple[int, int]] = set()
    for b, h in candidates:
        if not valid_pair(b, h):
            continue
        key = (b, h)
        if key in seen:
            continue
        seen.add(key)
        out.append({"bufsize": b, "hopsize": h})
    return out


def evaluate_with_cache(
    tracks: list[tuple[Path, Path, Path]],
    params: dict,
    cache: dict[tuple[int, int], dict],
) -> tuple[dict, bool]:
    key = params_key(params)
    cached = cache.get(key)
    if cached is not None:
        return cached, True
    result = evaluate_config(tracks, normalize_params(params))
    cache[key] = result
    return result, False


def main() -> None:
    parser = argparse.ArgumentParser(description="Aubio Ballroom benchmark")
    parser.add_argument("--dataset-root", default="benchmarks/ballroom-local", help="Local Ballroom dataset root")
    parser.add_argument("--output-root", default="benchmarks/ballroom-eval/results", help="Benchmark output root")
    parser.add_argument("--max-iterations", type=int, default=5, help="Maximum parameter update rounds")
    args = parser.parse_args()

    dataset_root = Path(args.dataset_root)
    output_root = Path(args.output_root)
    tracks = discover_tracks(dataset_root)
    if not tracks:
        raise SystemExit(f"No tracks found in {dataset_root}/audio with matching .beats and .bpm")

    run_id = dt.datetime.now().strftime("%Y%m%dT%H%M%SZ")
    run_dir = output_root / run_id
    run_dir.mkdir(parents=True, exist_ok=True)
    progress_path = run_dir / "progress.md"
    with progress_path.open("w", encoding="utf-8") as f:
        f.write("# Ballroom aubio fine-tuning progress\n\n")
        f.write(f"- Run ID: `{run_id}`\n")
        f.write(f"- Dataset root: `{dataset_root}`\n")
        f.write(f"- Tracks: **{len(tracks)}**\n")
        f.write("- Loop: **update params -> run aubio -> compare -> update -> repeat**\n")
        f.write("- Gate: **mean(F1@20ms, F1@35ms)**\n")
        f.write(f"- Max iterations: **{args.max_iterations}**\n\n")
        f.write(
            "| iter | config | bufsize | hopsize | F1@10ms | F1@20ms | F1@35ms | F1@50ms | F1@70ms | gate score | BPM MAE |\n"
        )
        f.write("|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n")

    cache: dict[tuple[int, int], dict] = {}
    all_results: list[dict] = []
    current, _ = evaluate_with_cache(tracks, BASELINE, cache)
    all_results.append(current)
    append_progress(progress_path, "baseline", 0, current)

    for iteration in range(1, args.max_iterations + 1):
        candidates = neighbor_configs(current["params"])
        if not candidates:
            append_iteration_note(progress_path, iteration, current, None, improved=False)
            break

        evaluated_neighbors: list[dict] = []
        for idx, cfg in enumerate(candidates, start=1):
            result, was_cached = evaluate_with_cache(tracks, cfg, cache)
            evaluated_neighbors.append(result)
            if not was_cached:
                all_results.append(result)
            label = f"iter-{iteration}-cand-{idx}"
            append_progress(progress_path, label, iteration, result)

        best_neighbor = max(evaluated_neighbors, key=score_tuple)
        if score_tuple(best_neighbor) > score_tuple(current):
            append_iteration_note(progress_path, iteration, current, best_neighbor, improved=True)
            current = best_neighbor
        else:
            append_iteration_note(progress_path, iteration, current, best_neighbor, improved=False)
            break

    best = current

    with (run_dir / "baseline_metrics.json").open("w", encoding="utf-8") as f:
        json.dump(cache[params_key(BASELINE)], f, indent=2)
    with (run_dir / "loop_metrics.json").open("w", encoding="utf-8") as f:
        json.dump(all_results, f, indent=2)
    with (run_dir / "best_config.json").open("w", encoding="utf-8") as f:
        json.dump(best, f, indent=2)

    latest = {
        "run_id": run_id,
        "dataset_root": str(dataset_root),
        "tracks_evaluated": len(tracks),
        "baseline": cache[params_key(BASELINE)]["aggregate"],
        "best": {
            "params": best["params"],
            "aggregate": best["aggregate"],
        },
    }
    with (output_root / "latest_summary.json").open("w", encoding="utf-8") as f:
        json.dump(latest, f, indent=2)

    print(f"Run: {run_id}")
    print(f"Tracks: {len(tracks)}")
    baseline = cache[params_key(BASELINE)]
    print(
        "Baseline gate score (mean F1@20ms/F1@35ms): "
        f"{((baseline['aggregate']['f1_20ms']['macro_f1'] + baseline['aggregate']['f1_35ms']['macro_f1']) / 2.0):.4f}"
    )
    print(f"Best params: {best['params']}")
    print(
        "Best gate score (mean F1@20ms/F1@35ms): "
        f"{((best['aggregate']['f1_20ms']['macro_f1'] + best['aggregate']['f1_35ms']['macro_f1']) / 2.0):.4f}"
    )
    print(f"Progress markdown: {progress_path}")


if __name__ == "__main__":
    main()
