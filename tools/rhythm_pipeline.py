"""
rhythm_pipeline.py
==================
Audio analysis tool — extracts musical features from audio files.

Outputs beat timestamps, per-band onset detection, mel-band flux scores,
intensity classification, and song structure. Does NOT make level design
decisions (obstacle types, lane assignments, difficulty filtering).

Uses librosa only.

Install:
    pip install librosa numpy

Usage:
    python rhythm_pipeline.py song.wav
    python rhythm_pipeline.py song.wav --output my_analysis.json
    python rhythm_pipeline.py song.wav --onset-threshold 0.5
"""

import argparse
import json
import sys
from pathlib import Path

import librosa
import numpy as np

DEFAULT_LIBROSA_CONFIG_PATH = Path(__file__).parent / "config" / "rhythm_librosa_params.json"

# ---------------------------------------------------------------------------
# ONSET DETECTION PASSES
#
# Multi-pass onset extraction over librosa mel flux envelopes.
# ---------------------------------------------------------------------------

ONSET_PASSES = [
    {"name": "kick", "method": "band_flux_bass", "zone": "bass"},
    {"name": "snare", "method": "band_flux_full", "zone": None},
    {"name": "melody", "method": "band_flux_low_mid", "zone": "low_mid"},
    {"name": "hihat", "method": "band_flux_high_mid", "zone": "high_mid"},
    {"name": "flux", "method": "spectral_flux", "zone": None},
]

# mel band indices (out of 40) that correspond to each frequency zone
# band 0-5   ~ bass (20-250hz)
# band 6-18  ~ low-mid (250-2000hz)
# band 19-30 ~ high-mid (2000-6000hz)
# band 31-39 ~ air (6000hz+)
MEL_ZONES = {
    "bass": (0, 5),
    "low_mid": (6, 18),
    "high_mid": (19, 30),
    "air": (31, 39),
}


def load_librosa_config(path: str | None) -> dict:
    if path is None:
        return {}
    cfg_path = Path(path)
    if not cfg_path.exists():
        print(f"Warning: librosa config not found: {cfg_path}; using built-in defaults", file=sys.stderr)
        return {}
    with cfg_path.open("r", encoding="utf-8") as f:
        return json.load(f)


def _section_cfg(config: dict | None, section: str) -> dict:
    if not config:
        return {}
    raw = config.get(section, {})
    return raw if isinstance(raw, dict) else {}


def onset_resolutions(config: dict | None = None) -> list[dict]:
    """
    Return onset analysis resolutions as a list of {"n_fft": int, "hop_length": int}.
    Uses onset.resolutions[] when present, otherwise falls back to onset n_fft/hop_length.
    """
    onset_cfg = _section_cfg(config, "onset")
    base_fft = int(onset_cfg.get("n_fft", 2048))
    base_hop = int(onset_cfg.get("hop_length", 512))
    resolutions_cfg = onset_cfg.get("resolutions", [])

    resolutions: list[dict] = []
    if isinstance(resolutions_cfg, list) and resolutions_cfg:
        for entry in resolutions_cfg:
            if not isinstance(entry, dict):
                continue
            n_fft = int(entry.get("n_fft", base_fft))
            hop_length = int(entry.get("hop_length", base_hop))
            if n_fft <= 0 or hop_length <= 0:
                continue
            resolutions.append({"n_fft": n_fft, "hop_length": hop_length})
    else:
        resolutions.append({"n_fft": base_fft, "hop_length": base_hop})
    return resolutions


def _mel_spectrogram_db(
    y: np.ndarray,
    sr: int,
    n_fft: int,
    hop_length: int,
    n_mels: int,
) -> np.ndarray:
    mel = librosa.feature.melspectrogram(
        y=y,
        sr=sr,
        n_fft=n_fft,
        hop_length=hop_length,
        n_mels=n_mels,
        power=2.0,
    )
    return librosa.power_to_db(mel + 1e-10, ref=np.max)


def _flux_envelope(mel_db: np.ndarray, zone: str | None) -> np.ndarray:
    if mel_db.shape[1] <= 1:
        return np.zeros((1,), dtype=np.float32)
    diff = np.maximum(mel_db[:, 1:] - mel_db[:, :-1], 0.0)
    if zone and zone in MEL_ZONES:
        lo, hi = MEL_ZONES[zone]
        diff = diff[lo:hi + 1, :]
    env = diff.mean(axis=0)
    env = np.concatenate([np.zeros((1,), dtype=env.dtype), env], axis=0)
    peak = float(np.max(env)) if env.size else 0.0
    return (env / peak) if peak > 1e-9 else env


def _detect_onsets_from_envelope(
    onset_env: np.ndarray,
    sr: int,
    hop_length: int,
    threshold: float,
) -> list[float]:
    if onset_env.size == 0:
        return []
    pre_max = 3
    post_max = 3
    pre_avg = 5
    post_avg = 5
    wait = 2
    peaks = librosa.util.peak_pick(
        onset_env,
        pre_max=pre_max,
        post_max=post_max,
        pre_avg=pre_avg,
        post_avg=post_avg,
        delta=float(threshold),
        wait=wait,
    )
    times = librosa.frames_to_time(peaks, sr=sr, hop_length=hop_length)
    return [float(t) for t in times]


def _detect_pass_onsets(
    y: np.ndarray,
    sr: int,
    zone: str | None,
    threshold: float,
    resolution: dict,
    n_mels: int,
) -> list[float]:
    n_fft = int(resolution["n_fft"])
    hop_length = int(resolution["hop_length"])
    mel_db = _mel_spectrogram_db(y, sr, n_fft=n_fft, hop_length=hop_length, n_mels=n_mels)
    onset_env = _flux_envelope(mel_db, zone)
    return _detect_onsets_from_envelope(onset_env, sr=sr, hop_length=hop_length, threshold=threshold)


def _spectral_flux_onset_envelope(
    y: np.ndarray,
    hop_length: int,
    n_fft: int,
    reduce: str = "mean",
) -> np.ndarray:
    mag = np.abs(librosa.stft(y=y, n_fft=n_fft, hop_length=hop_length))
    flux = np.diff(mag, axis=1)
    flux = np.maximum(flux, 0.0)
    if flux.shape[1] == 0:
        return np.zeros((1,), dtype=np.float32)
    reducer = reduce.lower()
    if reducer == "median":
        onset_env = np.median(flux, axis=0)
    elif reducer == "max":
        onset_env = np.max(flux, axis=0)
    else:
        onset_env = np.mean(flux, axis=0)
    return np.concatenate([np.zeros((1,), dtype=onset_env.dtype), onset_env], axis=0)


def _tempo_from_tempogram(
    onset_envelope: np.ndarray,
    sr: int,
    hop_length: int,
    win_length: int | None = None,
    norm: float | None = None,
) -> np.ndarray:
    tempo_kwargs: dict = {
        "onset_envelope": onset_envelope,
        "sr": sr,
        "hop_length": hop_length,
        "aggregate": None,
    }
    if win_length is not None or norm is not None:
        tg_kwargs: dict = {
            "onset_envelope": onset_envelope,
            "sr": sr,
            "hop_length": hop_length,
        }
        if win_length is not None:
            tg_kwargs["win_length"] = int(win_length)
        if norm is not None:
            tg_kwargs["norm"] = norm
        tempo_kwargs["tg"] = librosa.feature.tempogram(**tg_kwargs)
        tempo_kwargs.pop("onset_envelope", None)
    return librosa.feature.tempo(
        **tempo_kwargs,
    )


def get_tempo_and_beats(y: np.ndarray, sr: int, config: dict | None = None) -> tuple[float, list[float]]:
    beat_cfg = _section_cfg(config, "beat")
    n_fft = int(beat_cfg.get("n_fft", 2048))
    hop_length = int(beat_cfg.get("hop_length", 384))
    tightness = float(beat_cfg.get("tightness", 150.0))
    trim = bool(beat_cfg.get("trim", False))
    onset_envelope_kind = str(beat_cfg.get("onset_envelope", "librosa_default"))
    tempo_source = str(beat_cfg.get("tempo_source", "fixed_start_bpm"))
    flux_reduce = str(beat_cfg.get("flux_reduce", "mean"))
    tempo_stat = str(beat_cfg.get("tempo_stat", "median")).lower()
    tempogram_win_length = beat_cfg.get("tempogram_win_length")
    tempogram_norm_raw = beat_cfg.get("tempogram_norm")
    tempogram_norm = None if tempogram_norm_raw is None else float(tempogram_norm_raw)

    if onset_envelope_kind == "stft_flux":
        onset_env = _spectral_flux_onset_envelope(
            y=y,
            hop_length=hop_length,
            n_fft=n_fft,
            reduce=flux_reduce,
        )
    else:
        onset_env = librosa.onset.onset_strength(y=y, sr=sr, n_fft=n_fft, hop_length=hop_length)

    kwargs: dict = {
        "onset_envelope": onset_env,
        "sr": sr,
        "hop_length": hop_length,
        "tightness": tightness,
        "trim": trim,
    }
    if tempo_source == "fixed_start_bpm":
        if "start_bpm" in beat_cfg:
            kwargs["start_bpm"] = float(beat_cfg["start_bpm"])
    else:
        tempo_series = _tempo_from_tempogram(
            onset_envelope=onset_env,
            sr=sr,
            hop_length=hop_length,
            win_length=int(tempogram_win_length) if tempogram_win_length is not None else None,
            norm=tempogram_norm,
        )
        if tempo_source == "tempogram_dynamic":
            kwargs["bpm"] = tempo_series
        else:
            tempo_series_arr = np.asarray(tempo_series)
            if tempo_stat == "mean":
                kwargs["start_bpm"] = float(np.mean(tempo_series_arr))
            else:
                kwargs["start_bpm"] = float(np.median(tempo_series_arr))

    try:
        tempo, beat_frames = librosa.beat.beat_track(**kwargs)
    except Exception:
        if "bpm" in kwargs:
            tempo_series = kwargs.pop("bpm")
            kwargs["start_bpm"] = float(np.median(np.asarray(tempo_series)))
            tempo, beat_frames = librosa.beat.beat_track(**kwargs)
        else:
            raise

    beat_times = librosa.frames_to_time(beat_frames, sr=sr, hop_length=hop_length)
    tempo_value = float(np.asarray(tempo).reshape(-1)[0]) if np.asarray(tempo).size else 120.0
    return tempo_value, [float(t) for t in beat_times]


def get_quiet_regions(y: np.ndarray, sr: int, config: dict | None = None) -> list[dict]:
    """
    Build QUIET/NOISY transitions from RMS energy.
    Returns list of {"type": "QUIET"|"NOISY", "t": float}
    """
    quiet_cfg = _section_cfg(config, "quiet")
    frame_length = int(quiet_cfg.get("frame_length", 2048))
    hop_length = int(quiet_cfg.get("hop_length", 512))
    quantile = float(quiet_cfg.get("quantile", 0.2))
    min_duration_s = float(quiet_cfg.get("min_duration_s", 0.4))

    rms = librosa.feature.rms(y=y, frame_length=frame_length, hop_length=hop_length)[0]
    if rms.size == 0:
        return []
    times = librosa.frames_to_time(np.arange(rms.size), sr=sr, hop_length=hop_length)
    threshold = float(np.quantile(rms, quantile))
    quiet_mask = rms <= threshold

    # Smooth very short flips to avoid noisy micro-regions.
    min_frames = max(1, int(round(min_duration_s * sr / hop_length)))
    runs: list[tuple[int, int, bool]] = []
    start = 0
    cur = bool(quiet_mask[0])
    for i in range(1, quiet_mask.size):
        if bool(quiet_mask[i]) != cur:
            runs.append((start, i, cur))
            start = i
            cur = bool(quiet_mask[i])
    runs.append((start, quiet_mask.size, cur))

    merged: list[tuple[int, int, bool]] = []
    for run in runs:
        if merged and (run[1] - run[0]) < min_frames:
            prev = merged[-1]
            merged[-1] = (prev[0], run[1], prev[2])
        else:
            merged.append(run)

    regions: list[dict] = []
    for start_i, _, is_quiet in merged:
        regions.append(
            {
                "type": "QUIET" if is_quiet else "NOISY",
                "t": float(times[start_i]),
            }
        )
    return regions


def get_melbands(y: np.ndarray, sr: int, config: dict | None = None) -> tuple[np.ndarray, np.ndarray]:
    mel_cfg = _section_cfg(config, "mel")
    n_mels = int(mel_cfg.get("n_mels", 40))
    n_fft = int(mel_cfg.get("n_fft", 2048))
    hop_length = int(mel_cfg.get("hop_length", 512))

    mel_db = _mel_spectrogram_db(y, sr, n_fft=n_fft, hop_length=hop_length, n_mels=n_mels)
    timestamps = librosa.frames_to_time(np.arange(mel_db.shape[1]), sr=sr, hop_length=hop_length)
    return np.asarray(timestamps, dtype=np.float64), mel_db.T


def get_mfcc(y: np.ndarray, sr: int, config: dict | None = None) -> tuple[np.ndarray, np.ndarray]:
    mfcc_cfg = _section_cfg(config, "mfcc")
    n_mfcc = int(mfcc_cfg.get("n_mfcc", 13))
    n_fft = int(mfcc_cfg.get("n_fft", 2048))
    hop_length = int(mfcc_cfg.get("hop_length", 512))

    mfcc = librosa.feature.mfcc(
        y=y,
        sr=sr,
        n_mfcc=n_mfcc,
        n_fft=n_fft,
        hop_length=hop_length,
    )
    timestamps = librosa.frames_to_time(np.arange(mfcc.shape[1]), sr=sr, hop_length=hop_length)
    return np.asarray(timestamps, dtype=np.float64), mfcc.T


# ---------------------------------------------------------------------------
# STEP 1 - EXTRACT ALL FEATURES
# ---------------------------------------------------------------------------

def extract_features(filepath: str, onset_threshold: float, librosa_config: dict | None = None) -> dict:
    print(f"\n[1] Extracting features from: {filepath}")
    y, sr = librosa.load(filepath, sr=None, mono=True)
    duration = float(librosa.get_duration(y=y, sr=sr))

    print("    tempo + beats...")
    bpm, beats = get_tempo_and_beats(y, sr, config=librosa_config)
    print(f"    BPM: {bpm:.1f}  |  beats: {len(beats)}")

    print("    onsets per method...")
    onset_cfg = _section_cfg(librosa_config, "onset")
    n_mels = int(onset_cfg.get("n_mels", 40))
    onsets: dict[str, list[float]] = {}
    onset_breakdown: dict[str, list[dict]] = {}
    resolutions = onset_resolutions(librosa_config)
    for p in ONSET_PASSES:
        combined: list[float] = []
        per_resolution: list[dict] = []
        for res in resolutions:
            detected = _detect_pass_onsets(
                y=y,
                sr=sr,
                zone=p["zone"],
                threshold=onset_threshold,
                resolution=res,
                n_mels=n_mels,
            )
            per_resolution.append(
                {
                    "n_fft": int(res["n_fft"]),
                    "hop_length": int(res["hop_length"]),
                    "count": len(detected),
                }
            )
            combined.extend(detected)

        # De-duplicate exact-frame overlaps across resolutions.
        o = sorted({round(t, 6) for t in combined})
        onsets[p["name"]] = o
        onset_breakdown[p["name"]] = per_resolution
        print(
            f"      {p['name']:8s} ({p['method']:16s}): {len(o)} onsets "
            f"across {len(per_resolution)} res"
        )

    print("    mel band energies...")
    mel_times, mel_energies = get_melbands(y, sr, config=librosa_config)
    print(f"    mel frames: {len(mel_times)}  bands: {mel_energies.shape[1] if len(mel_energies) else 0}")

    print("    MFCC coefficients...")
    mfcc_times, mfcc_coeffs = get_mfcc(y, sr, config=librosa_config)
    print(f"    mfcc frames: {len(mfcc_times)}  coeffs: {mfcc_coeffs.shape[1] if len(mfcc_coeffs) else 0}")

    print("    quiet regions...")
    quiet = get_quiet_regions(y, sr, config=librosa_config)

    # song duration = track duration if available
    all_times = beats + [t for v in onsets.values() for t in v]
    computed_duration = max(all_times) + 1.0 if all_times else duration

    return {
        "bpm": bpm,
        "beats": beats,
        "onsets": onsets,
        "onset_resolutions": resolutions,
        "onset_breakdown": onset_breakdown,
        "mel_times": mel_times,
        "mel_energies": mel_energies,
        "mfcc_times": mfcc_times,
        "mfcc_coeffs": mfcc_coeffs,
        "quiet": quiet,
        "duration": max(duration, computed_duration),
    }


# ---------------------------------------------------------------------------
# STEP 2 - COMPUTE PER-ONSET FLUX FROM MEL BANDS
# ---------------------------------------------------------------------------

def compute_onset_flux(
    t: float,
    mel_times: np.ndarray,
    mel_energies: np.ndarray,
    zone: str = None,
) -> float:
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
        cur = mel_energies[idx, lo : hi + 1]
        prev = mel_energies[idx - 1, lo : hi + 1]
    else:
        cur = mel_energies[idx]
        prev = mel_energies[idx - 1]

    flux = float(np.sum(np.maximum(cur - prev, 0)))
    return flux


def assign_flux_to_onsets(features: dict) -> list[dict]:
    """
    For each onset in each pass, compute its flux score from the mel bands.
    Returns flat list of event dicts sorted by time.
    """
    mel_times = features["mel_times"]
    mel_energies = features["mel_energies"]

    events = []
    for p in ONSET_PASSES:
        zone = p["zone"]
        for t in features["onsets"][p["name"]]:
            flux = compute_onset_flux(t, mel_times, mel_energies, zone)
            events.append(
                {
                    "t": round(t, 3),
                    "pass": p["name"],
                    "method": p["method"],
                    "flux": round(flux, 4),
                }
            )

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

        t_rep = round(sum(e["t"] for e in group) / len(group), 3)
        passes = sorted({e["pass"] for e in group})
        max_flux = round(max(e["flux"] for e in group), 4)

        merged.append(
            {
                "t": t_rep,
                "flux": max_flux,
                "passes": passes,
            }
        )
        i = j

    return merged


# ---------------------------------------------------------------------------
# STEP 4 - CLASSIFY BY INTENSITY (mel band energy -> easy/medium/hard filter)
# ---------------------------------------------------------------------------

def classify_intensity(events: list[dict], quiet_regions: list[dict]) -> list[dict]:
    """
    Tag each event with intensity (low / medium / high) based on:
    - Whether it falls in a quiet region (RMS-derived quiet output)
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
    p33 = float(np.percentile(fluxes, 33))
    p66 = float(np.percentile(fluxes, 66))

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
    Falls back to quiet-region output if MFCC data is unavailable.

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
    bpm = max(1.0, features["bpm"])
    frame_rate = 1.0 / (mfcc_times[1] - mfcc_times[0]) if len(mfcc_times) > 1 else 172.0
    beat_period = 60.0 / bpm
    hop = max(1, int(beat_period * frame_rate))

    t_ds = mfcc_times[::hop]
    m_ds = mfcc_coeffs[::hop]

    # ── Compute novelty curve (cosine distance between windows) ──
    win = 4  # 4-beat window for smoothing
    novelty = []
    for i in range(win, len(m_ds) - win):
        before = m_ds[i - win : i].mean(axis=0)
        after = m_ds[i : i + win].mean(axis=0)
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
        structure.append(
            {
                "section": label,
                "start": round(start, 2),
                "end": round(end, 2),
                "intensity": intensity,
            }
        )

    return structure


def _label_sections(
    boundaries: list[float],
    duration: float,
    mel_times: np.ndarray,
    mel_energies: np.ndarray,
) -> list[tuple[str, str]]:
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
    """Fallback: quiet-based segmentation."""
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
        structure.append(
            {
                "section": section,
                "start": round(seg["start"], 2),
                "end": round(seg["end"], 2),
                "intensity": intensity,
            }
        )

    return structure


# ---------------------------------------------------------------------------
# ASSEMBLE ANALYSIS OUTPUT
# ---------------------------------------------------------------------------

def build_analysis(filepath: str, features: dict, onset_threshold: float) -> dict:
    print(f"\n[2] Assigning flux scores from mel bands...")
    events = assign_flux_to_onsets(features)
    print(f"    total raw events: {len(events)}")

    print("[3] Merging overlapping events (window=50ms)...")
    events = merge_events(events)
    print(f"    after merge: {len(events)}")

    print("[4] Classifying intensity from quiet regions...")
    events = classify_intensity(events, features["quiet"])

    print("[5] Building song structure...")
    structure = build_structure(features)

    # per-pass onset summary
    pass_summary = {}
    for p in ONSET_PASSES:
        timestamps = [round(t, 3) for t in features["onsets"][p["name"]]]
        pass_summary[p["name"]] = {
            "method": p["method"],
            "zone": p["zone"],
            "count": len(timestamps),
            "resolutions": features.get("onset_breakdown", {}).get(p["name"], []),
            "timestamps": timestamps,
        }

    # flux percentile stats for downstream consumers
    fluxes = [e["flux"] for e in events]
    flux_stats = {}
    if fluxes:
        flux_arr = np.array(fluxes)
        flux_stats = {
            "min": round(float(np.min(flux_arr)), 4),
            "max": round(float(np.max(flux_arr)), 4),
            "mean": round(float(np.mean(flux_arr)), 4),
            "p25": round(float(np.percentile(flux_arr, 25)), 4),
            "p50": round(float(np.percentile(flux_arr, 50)), 4),
            "p75": round(float(np.percentile(flux_arr, 75)), 4),
            "p90": round(float(np.percentile(flux_arr, 90)), 4),
        }

    # summary
    intensity_counts = {}
    for e in events:
        intensity_counts[e["intensity"]] = intensity_counts.get(e["intensity"], 0) + 1
    print(f"\n    event summary: {len(events)} events")
    for k, v in sorted(intensity_counts.items()):
        print(f"      {k:8s}: {v}")

    return {
        "title": Path(filepath).stem,
        "source": Path(filepath).name,
        "bpm": round(features["bpm"], 2),
        "duration": round(features["duration"], 2),
        "onset_threshold": onset_threshold,
        "beats": [round(b, 3) for b in features["beats"]],
        "structure": structure,
        "onsets": pass_summary,
        "flux_stats": flux_stats,
        "events": events,
        "quiet_regions": features["quiet"],
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
        "--output",
        "-o",
        default=None,
        help="Output JSON path (default: <input_stem>_analysis.json)",
    )
    parser.add_argument(
        "--onset-threshold",
        "-t",
        type=float,
        default=None,
        help="Onset peak-pick threshold 0.01-1.0 (default: from config, fallback 0.3)",
    )
    parser.add_argument(
        "--librosa-config",
        default=str(DEFAULT_LIBROSA_CONFIG_PATH),
        help="JSON config for librosa beat/onset params (default: tools/config/rhythm_librosa_params.json)",
    )
    args = parser.parse_args()

    filepath = args.input
    if not Path(filepath).exists():
        print(f"Error: file not found: {filepath}", file=sys.stderr)
        sys.exit(1)

    librosa_config = load_librosa_config(args.librosa_config)
    onset_threshold = args.onset_threshold
    if onset_threshold is None:
        onset_threshold = float(_section_cfg(librosa_config, "onset").get("threshold", 0.3))

    if not (0.01 <= onset_threshold <= 1.0):
        print(
            f"Error: onset-threshold must be between 0.01 and 1.0, got {onset_threshold}",
            file=sys.stderr,
        )
        sys.exit(1)

    print("=" * 60)
    print("  RHYTHM PIPELINE — Audio Analysis")
    print("  librosa mel spectrogram -> onset detection -> flux -> intensity")
    print("=" * 60)

    features = extract_features(filepath, onset_threshold, librosa_config=librosa_config)
    analysis = build_analysis(filepath, features, onset_threshold)
    analysis["librosa_params"] = {
        "config_path": str(args.librosa_config),
        "beat": _section_cfg(librosa_config, "beat"),
        "onset": {
            **_section_cfg(librosa_config, "onset"),
            "threshold": onset_threshold,
        },
        "mel": _section_cfg(librosa_config, "mel"),
        "mfcc": _section_cfg(librosa_config, "mfcc"),
        "quiet": _section_cfg(librosa_config, "quiet"),
    }

    out_path = args.output or f"{Path(filepath).stem}_analysis.json"
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(analysis, f, indent=2)
    print(f"\n✓ {out_path}  ({len(analysis['events'])} events)")
    print("=" * 60)


if __name__ == "__main__":
    main()
