"""
rhythm_pipeline.py
==================
Audio analysis tool — extracts musical features from audio files.

Outputs beat timestamps, per-band onset detection, mel-band flux scores,
intensity classification, and song structure. Does NOT make level design
decisions (obstacle types, lane assignments, difficulty filtering).

Uses aubio CLI tools only. No librosa required.

Install:
    pip install aubio numpy

Usage:
    python rhythm_pipeline.py song.wav
    python rhythm_pipeline.py song.wav --output my_analysis.json
    python rhythm_pipeline.py song.wav --onset-threshold 0.5
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

import numpy as np


# ---------------------------------------------------------------------------
# ONSET DETECTION PASSES
#
# aubio onset methods, each targeting a different frequency/sound type:
#   hfc      - high frequency content — kick drums, bass attacks
#   complex  - complex domain — broadband transients (snare, clap)
#   phase    - phase deviation — melodic/harmonic onsets
#   mkl      - kullback-leibler — hi-hats, cymbals, percussive highs
#
# we run 4 separate onset passes to capture different sound layers,
# then merge into unified onset events with flux scores.
# ---------------------------------------------------------------------------

ONSET_PASSES = [
    {"name": "kick",   "method": "hfc",      "zone": "bass"},
    {"name": "snare",  "method": "complex",   "zone": None},
    {"name": "melody", "method": "phase",     "zone": "low_mid"},
    {"name": "hihat",  "method": "mkl",       "zone": "high_mid"},
    {"name": "flux",   "method": "specflux",  "zone": None},
]

# mel band indices (out of 40) that correspond to each frequency zone
# aubio melbands outputs 40 bands from ~20hz to nyquist
# band 0-5   ~ bass (20-250hz)
# band 6-18  ~ low-mid (250-2000hz)
# band 19-30 ~ high-mid (2000-6000hz)
# band 31-39 ~ air (6000hz+)
MEL_ZONES = {
    "bass":     (0,  5),
    "low_mid":  (6,  18),
    "high_mid": (19, 30),
    "air":      (31, 39),
}


# ---------------------------------------------------------------------------
# AUBIO CLI WRAPPERS
# ---------------------------------------------------------------------------

def run_aubio(command: str, filepath: str, extra_args: list = None) -> str:
    """Run an aubio CLI command and return stdout."""
    cmd = ["aubio", command, filepath]
    if extra_args:
        cmd.extend(extra_args)
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(f"aubio {command} failed: {result.stderr.strip()}")
    return result.stdout


def parse_timestamps(output: str) -> list[float]:
    """Parse aubio timestamp output (one float per line, tab-separated)."""
    times = []
    for line in output.strip().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            t = float(line.split("\t")[0])
            times.append(t)
        except ValueError:
            continue
    return times


def get_tempo(filepath: str) -> float:
    """Get BPM via aubio tempo."""
    out = run_aubio("tempo", filepath)
    for line in out.splitlines():
        try:
            return float(line.strip().split()[0])
        except (ValueError, IndexError):
            continue
    return 120.0


def get_beats(filepath: str) -> list[float]:
    """Get beat timestamps via aubio beat."""
    out = run_aubio("beat", filepath)
    return parse_timestamps(out)


def get_onsets(filepath: str, method: str, threshold: float = 0.3) -> list[float]:
    """Get onset timestamps for a given aubio onset method."""
    out = run_aubio("onset", filepath, ["-m", method, "-t", str(threshold)])
    return parse_timestamps(out)


def get_quiet_regions(filepath: str) -> list[dict]:
    """
    Get quiet/noisy regions via aubio quiet.
    aubio outputs lines like: "QUIET: 143.168435\\t"
    Returns list of {"type": "QUIET"|"NOISY", "t": float}
    """
    out = run_aubio("quiet", filepath)
    regions = []
    for line in out.strip().splitlines():
        line = line.strip()
        if not line:
            continue
        # format is "LABEL: TIMESTAMP\t" — split on ": " to get label and time
        if ": " in line:
            label, rest = line.split(": ", 1)
            try:
                t = float(rest.strip())
                regions.append({"type": label, "t": t})
            except ValueError:
                continue
    return regions


def get_melbands(filepath: str) -> tuple[np.ndarray, np.ndarray]:
    """
    Get mel band energies via aubio melbands.
    Returns (timestamps, energies) where energies is shape (n_frames, 40).
    """
    out = run_aubio("melbands", filepath)
    timestamps = []
    energies = []
    for line in out.strip().splitlines():
        parts = line.strip().split("\t")
        if len(parts) < 2:
            continue
        try:
            t = float(parts[0])
            bands = [float(v) for v in parts[1].split()]
            timestamps.append(t)
            energies.append(bands)
        except ValueError:
            continue
    return np.array(timestamps), np.array(energies)


def get_mfcc(filepath: str) -> tuple[np.ndarray, np.ndarray]:
    """
    Get MFCC features via aubio mfcc.
    Returns (timestamps, coefficients) where coefficients is shape (n_frames, 13).
    """
    out = run_aubio("mfcc", filepath)
    timestamps = []
    coefficients = []
    for line in out.strip().splitlines():
        parts = line.strip().split("\t")
        if len(parts) < 2:
            continue
        try:
            t = float(parts[0])
            coeffs = [float(v) for v in parts[1].split()]
            timestamps.append(t)
            coefficients.append(coeffs)
        except ValueError:
            continue
    return np.array(timestamps), np.array(coefficients)


# ---------------------------------------------------------------------------
# STEP 1 - EXTRACT ALL FEATURES
# ---------------------------------------------------------------------------

def extract_features(filepath: str, onset_threshold: float) -> dict:
    print(f"\n[1] Extracting features from: {filepath}")

    print("    tempo + beats...")
    bpm   = get_tempo(filepath)
    beats = get_beats(filepath)
    print(f"    BPM: {bpm:.1f}  |  beats: {len(beats)}")

    print("    onsets per method...")
    onsets = {}
    for p in ONSET_PASSES:
        o = get_onsets(filepath, p["method"], threshold=onset_threshold)
        onsets[p["name"]] = o
        print(f"      {p['name']:8s} ({p['method']:8s}): {len(o)} onsets")

    print("    mel band energies...")
    mel_times, mel_energies = get_melbands(filepath)
    print(f"    mel frames: {len(mel_times)}  bands: {mel_energies.shape[1] if len(mel_energies) else 0}")

    print("    MFCC coefficients...")
    mfcc_times, mfcc_coeffs = get_mfcc(filepath)
    print(f"    mfcc frames: {len(mfcc_times)}  coeffs: {mfcc_coeffs.shape[1] if len(mfcc_coeffs) else 0}")

    print("    quiet regions...")
    quiet = get_quiet_regions(filepath)

    # song duration = last beat or last onset, whichever is later
    all_times = beats + [t for v in onsets.values() for t in v]
    duration = max(all_times) + 1.0 if all_times else 0.0

    return {
        "bpm":          bpm,
        "beats":        beats,
        "onsets":       onsets,
        "mel_times":    mel_times,
        "mel_energies": mel_energies,
        "mfcc_times":   mfcc_times,
        "mfcc_coeffs":  mfcc_coeffs,
        "quiet":        quiet,
        "duration":     duration,
    }


# ---------------------------------------------------------------------------
# STEP 2 - COMPUTE PER-ONSET FLUX FROM MEL BANDS
# ---------------------------------------------------------------------------

def compute_onset_flux(t: float, mel_times: np.ndarray, mel_energies: np.ndarray,
                        zone: str = None) -> float:
    """
    Look up the mel-band energy flux (rate of change) at time t.
    If zone is given, only use that subset of mel bands.
    """
    if len(mel_times) == 0:
        return 0.0

    idx = int(np.searchsorted(mel_times, t))
    idx = np.clip(idx, 1, len(mel_times) - 1)

    if zone and zone in MEL_ZONES:
        lo, hi = MEL_ZONES[zone]
        cur  = mel_energies[idx,   lo:hi+1]
        prev = mel_energies[idx-1, lo:hi+1]
    else:
        cur  = mel_energies[idx]
        prev = mel_energies[idx-1]

    flux = float(np.sum(np.maximum(cur - prev, 0)))
    return flux


def assign_flux_to_onsets(features: dict) -> list[dict]:
    """
    For each onset in each pass, compute its flux score from the mel bands.
    Returns flat list of event dicts sorted by time.
    """
    mel_times    = features["mel_times"]
    mel_energies = features["mel_energies"]

    events = []
    for p in ONSET_PASSES:
        zone = p["zone"]
        for t in features["onsets"][p["name"]]:
            flux = compute_onset_flux(t, mel_times, mel_energies, zone)
            events.append({
                "t":      round(t, 3),
                "pass":   p["name"],
                "method": p["method"],
                "flux":   round(flux, 4),
            })

    events.sort(key=lambda e: e["t"])
    return events


# ---------------------------------------------------------------------------
# STEP 3 - MERGE OVERLAPPING EVENTS (same musical moment)
# ---------------------------------------------------------------------------

def merge_events(events: list[dict], merge_window: float = 0.05) -> list[dict]:
    """
    Collapse events within merge_window seconds into a single event.
    When multiple passes fire together, keep the highest-flux one as primary
    and record all contributing passes.
    """
    merged = []
    i = 0
    while i < len(events):
        group = [events[i]]
        j = i + 1
        while j < len(events) and events[j]["t"] - events[i]["t"] < merge_window:
            group.append(events[j])
            j += 1

        t_rep    = round(sum(e["t"] for e in group) / len(group), 3)
        passes   = sorted({e["pass"] for e in group})
        max_flux = round(max(e["flux"] for e in group), 4)

        merged.append({
            "t":     t_rep,
            "flux":  max_flux,
            "passes": passes,
        })
        i = j

    return merged


# ---------------------------------------------------------------------------
# STEP 4 - CLASSIFY BY INTENSITY (mel band energy -> easy/medium/hard filter)
# ---------------------------------------------------------------------------

def classify_intensity(events: list[dict], quiet_regions: list[dict]) -> list[dict]:
    """
    Tag each event with intensity (low / medium / high) based on:
    - Whether it falls in a quiet region (aubio quiet output)
    - Its flux score relative to the overall distribution
    """
    # build quiet time windows: a QUIET region runs from a QUIET marker
    # to the next NOISY marker
    quiet_windows = []
    quiet_start = None
    for r in quiet_regions:
        if r["type"] == "QUIET":
            quiet_start = r["t"]
        elif r["type"] == "NOISY" and quiet_start is not None:
            quiet_windows.append((quiet_start, r["t"]))
            quiet_start = None

    def is_quiet(t):
        for (a, b) in quiet_windows:
            if a <= t <= b:
                return True
        return False

    fluxes = np.array([e["flux"] for e in events]) if events else np.array([0.0])
    p33    = float(np.percentile(fluxes, 33))
    p66    = float(np.percentile(fluxes, 66))

    for e in events:
        if is_quiet(e["t"]):
            e["intensity"] = "low"
        elif e["flux"] >= p66:
            e["intensity"] = "high"
        elif e["flux"] >= p33:
            e["intensity"] = "medium"
        else:
            e["intensity"] = "low"

    return events


# ---------------------------------------------------------------------------
# STEP 5 - BUILD SONG STRUCTURE FROM QUIET REGIONS + BEATS
# ---------------------------------------------------------------------------

def build_structure(features: dict) -> list[dict]:
    """
    Detect song sections using MFCC self-similarity analysis.

    Computes a timbral novelty curve from MFCC cosine distance between
    consecutive windows. Peaks in the novelty curve mark section boundaries.
    Falls back to aubio quiet output if MFCC data is unavailable.

    Sections are labeled by position and energy:
      intro → verse → pre-chorus → chorus → bridge → drop → outro
    """
    duration = features["duration"]
    mfcc_times = features.get("mfcc_times", np.array([]))
    mfcc_coeffs = features.get("mfcc_coeffs", np.array([]))
    mel_times = features["mel_times"]
    mel_energies = features["mel_energies"]

    if len(mfcc_times) < 100:
        # Fallback: simple quiet-based segmentation
        return _build_structure_from_quiet(features)

    # ── Downsample MFCC to ~1 frame per beat ─────────────────
    bpm = features["bpm"]
    frame_rate = 1.0 / (mfcc_times[1] - mfcc_times[0]) if len(mfcc_times) > 1 else 172.0
    beat_period = 60.0 / bpm
    hop = max(1, int(beat_period * frame_rate))

    t_ds = mfcc_times[::hop]
    m_ds = mfcc_coeffs[::hop]

    # ── Compute novelty curve (cosine distance between windows) ──
    win = 4  # 4-beat window for smoothing
    novelty = []
    for i in range(win, len(m_ds) - win):
        before = m_ds[i - win:i].mean(axis=0)
        after = m_ds[i:i + win].mean(axis=0)
        norm_b = np.linalg.norm(before)
        norm_a = np.linalg.norm(after)
        if norm_b < 1e-10 or norm_a < 1e-10:
            dist = 0.0
        else:
            dist = 1.0 - np.dot(before, after) / (norm_b * norm_a)
        novelty.append((float(t_ds[i]), float(dist)))

    if not novelty:
        return _build_structure_from_quiet(features)

    # ── Find peaks (section boundaries) ──────────────────────
    nov_vals = np.array([n[1] for n in novelty])
    threshold = float(np.percentile(nov_vals, 85))
    min_section_len = 5.0  # minimum 5s between boundaries

    boundaries = [0.0]
    for i, (t, n) in enumerate(novelty):
        if n > threshold:
            is_peak = True
            if i > 0 and novelty[i - 1][1] > n:
                is_peak = False
            if i < len(novelty) - 1 and novelty[i + 1][1] > n:
                is_peak = False
            if is_peak and (t - boundaries[-1]) >= min_section_len:
                boundaries.append(t)
    boundaries.append(duration)

    # ── Label sections by position + energy ──────────────────
    # Compute average mel energy per section for intensity classification
    structure = []
    section_labels = _label_sections(boundaries, duration, mel_times, mel_energies)

    for i in range(len(boundaries) - 1):
        start = boundaries[i]
        end = boundaries[i + 1]
        label, intensity = section_labels[i]
        structure.append({
            "section": label,
            "start": round(start, 2),
            "end": round(end, 2),
            "intensity": intensity,
        })

    return structure


def _label_sections(boundaries: list[float], duration: float,
                    mel_times: np.ndarray, mel_energies: np.ndarray) -> list[tuple[str, str]]:
    """
    Label each section based on position in song and energy level.
    Returns list of (section_name, intensity) tuples.
    """
    n = len(boundaries) - 1
    if n == 0:
        return [("verse", "medium")]

    # Compute average energy per section
    energies = []
    for i in range(n):
        start = boundaries[i]
        end = boundaries[i + 1]
        mask = (mel_times >= start) & (mel_times < end)
        if np.any(mask):
            avg = float(np.mean(mel_energies[mask]))
        else:
            avg = 0.0
        energies.append(avg)

    if not energies or max(energies) == 0:
        return [("verse", "medium")] * n

    # Normalize energies to 0-1
    e_arr = np.array(energies)
    e_norm = (e_arr - e_arr.min()) / (e_arr.max() - e_arr.min() + 1e-10)

    labels = []
    for i in range(n):
        pos = boundaries[i] / duration  # 0.0 to 1.0
        energy = e_norm[i]

        # Intensity from energy
        if energy < 0.3:
            intensity = "low"
        elif energy < 0.7:
            intensity = "medium"
        else:
            intensity = "high"

        # Section label from position + energy
        if pos < 0.08:
            section = "intro"
        elif pos > 0.90:
            section = "outro"
        elif energy >= 0.7:
            # High energy: chorus or drop
            if boundaries[i + 1] - boundaries[i] < 15:
                section = "drop"
            else:
                section = "chorus"
        elif energy < 0.3:
            section = "bridge"
        else:
            # Medium energy: verse or pre-chorus
            # Pre-chorus if next section is high energy
            if i + 1 < n and e_norm[i + 1] >= 0.7:
                section = "pre-chorus"
            else:
                section = "verse"

        labels.append((section, intensity))

    return labels


def _build_structure_from_quiet(features: dict) -> list[dict]:
    """Fallback: original quiet-based segmentation."""
    duration = features["duration"]
    quiet = features["quiet"]

    if not quiet:
        return [{"section": "verse", "start": 0.0, "end": round(duration, 2), "intensity": "medium"}]

    segments = []
    state = "NOISY"
    last_t = 0.0
    for r in quiet:
        if r["type"] != state:
            segments.append({"state": state, "start": last_t, "end": r["t"]})
            last_t = r["t"]
            state = r["type"]
    segments.append({"state": state, "start": last_t, "end": duration})

    structure = []
    for seg in segments:
        pos = seg["start"] / duration
        if seg["state"] == "QUIET":
            section = "intro" if pos < 0.15 else ("outro" if pos > 0.85 else "bridge")
            intensity = "low"
        else:
            section = "verse" if pos < 0.3 else "chorus"
            intensity = "medium" if pos < 0.3 else "high"
        structure.append({
            "section": section,
            "start": round(seg["start"], 2),
            "end": round(seg["end"], 2),
            "intensity": intensity,
        })

    return structure


# ---------------------------------------------------------------------------
# ASSEMBLE ANALYSIS OUTPUT
# ---------------------------------------------------------------------------

def build_analysis(filepath: str, features: dict,
                   onset_threshold: float) -> dict:
    print(f"\n[2] Assigning flux scores from mel bands...")
    events = assign_flux_to_onsets(features)
    print(f"    total raw events: {len(events)}")

    print(f"[3] Merging overlapping events (window=50ms)...")
    events = merge_events(events)
    print(f"    after merge: {len(events)}")

    print(f"[4] Classifying intensity from quiet regions...")
    events = classify_intensity(events, features["quiet"])

    print(f"[5] Building song structure...")
    structure = build_structure(features)

    # per-pass onset summary
    pass_summary = {}
    for p in ONSET_PASSES:
        timestamps = [round(t, 3) for t in features["onsets"][p["name"]]]
        pass_summary[p["name"]] = {
            "method": p["method"],
            "zone":   p["zone"],
            "count":  len(timestamps),
            "timestamps": timestamps,
        }

    # flux percentile stats for downstream consumers
    fluxes = [e["flux"] for e in events]
    flux_stats = {}
    if fluxes:
        flux_arr = np.array(fluxes)
        flux_stats = {
            "min":  round(float(np.min(flux_arr)), 4),
            "max":  round(float(np.max(flux_arr)), 4),
            "mean": round(float(np.mean(flux_arr)), 4),
            "p25":  round(float(np.percentile(flux_arr, 25)), 4),
            "p50":  round(float(np.percentile(flux_arr, 50)), 4),
            "p75":  round(float(np.percentile(flux_arr, 75)), 4),
            "p90":  round(float(np.percentile(flux_arr, 90)), 4),
        }

    # summary
    intensity_counts = {}
    for e in events:
        intensity_counts[e["intensity"]] = intensity_counts.get(e["intensity"], 0) + 1
    print(f"\n    event summary: {len(events)} events")
    for k, v in sorted(intensity_counts.items()):
        print(f"      {k:8s}: {v}")

    return {
        "title":           Path(filepath).stem,
        "source":          Path(filepath).name,
        "bpm":             round(features["bpm"], 2),
        "duration":        round(features["duration"], 2),
        "onset_threshold": onset_threshold,
        "beats":           [round(b, 3) for b in features["beats"]],
        "structure":       structure,
        "onsets":          pass_summary,
        "flux_stats":      flux_stats,
        "events":          events,
        "quiet_regions":   features["quiet"],
    }


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Audio analysis tool — extracts musical features for level design"
    )
    parser.add_argument("input", help="Path to audio file (.wav, .mp3, .flac, ...)")
    parser.add_argument(
        "--output", "-o",
        default=None,
        help="Output JSON path (default: <input_stem>_analysis.json)"
    )
    parser.add_argument(
        "--onset-threshold", "-t",
        type=float,
        default=0.3,
        help="aubio onset detection threshold 0.1-0.9 (default: 0.3, lower=more sensitive)"
    )
    args = parser.parse_args()

    filepath = args.input
    if not Path(filepath).exists():
        print(f"Error: file not found: {filepath}", file=sys.stderr)
        sys.exit(1)

    print("=" * 60)
    print("  RHYTHM PIPELINE — Audio Analysis")
    print("  mel spectrogram -> onset detection -> flux -> intensity")
    print("=" * 60)

    features = extract_features(filepath, args.onset_threshold)
    analysis = build_analysis(filepath, features, args.onset_threshold)

    out_path = args.output or f"{Path(filepath).stem}_analysis.json"
    with open(out_path, "w") as f:
        json.dump(analysis, f, indent=2)
    print(f"\n✓ {out_path}  ({len(analysis['events'])} events)")
    print("=" * 60)


if __name__ == "__main__":
    main()