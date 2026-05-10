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
#
# Pass identifiers are deliberately non-instrumental.  Each ID encodes the
# broad layer it belongs to plus the spectral zone or method (so multiple
# internal passes remain distinguishable in diagnostics) WITHOUT implying
# real instrument detection.  The audio analysis only has broad librosa
# evidence (percussive / harmonic / full-spectrum), not raw kick / snare /
# hihat / melody recognition.
# ---------------------------------------------------------------------------

# The ``method`` field semantically dispatches the detection envelope:
#   - "band_flux_*"  : mel-band positive flux envelope, optionally restricted
#                      to a frequency zone (driven by ``zone``).
#   - "spectral_flux": full STFT positive spectral flux envelope, computed
#                      via ``_spectral_flux_onset_envelope``.  This is a
#                      genuinely independent envelope from the mel-band path
#                      and must NOT be a duplicate of any band_flux pass.
ONSET_PASSES = [
    {"name": "percussive_bass",      "method": "band_flux_bass",     "zone": "bass"},
    {"name": "percussive_broadband", "method": "band_flux_full",     "zone": None},
    {"name": "harmonic_low_mid",     "method": "band_flux_low_mid",  "zone": "low_mid"},
    {"name": "percussive_high_mid",  "method": "band_flux_high_mid", "zone": "high_mid"},
    {"name": "full_spectrum_flux",   "method": "spectral_flux",      "zone": None},
]

# ---------------------------------------------------------------------------
# BROAD LAYER CLASSES
#
# Maps the named detection passes to the three broad librosa layer classes
# used for lane/shape assignment.  These classes must NOT be merged across
# each other even when their timestamps are very close (< 50 ms).
#
#   percussive    — bass / broadband / high-mid → lane 0, triangle
#   harmonic      — low-mid                     → lane 2, circle
#   full-spectrum — spectral flux (catch-all)   → lane 1, square
#
# Legacy aliases (kick / snare / hihat / melody / flux) are kept ONLY so
# previously generated analysis JSON files still resolve to the right
# layer when read back.  Newly generated analysis output uses the broad
# subpass IDs above and never serializes the legacy aliases.
# ---------------------------------------------------------------------------

PASS_TO_LAYER: dict[str, str] = {
    "percussive_bass":      "percussive",
    "percussive_broadband": "percussive",
    "percussive_high_mid":  "percussive",
    "harmonic_low_mid":     "harmonic",
    "full_spectrum_flux":   "full-spectrum",
    # Legacy aliases (read-only fallback for older analysis JSONs).
    "kick":   "percussive",
    "snare":  "percussive",
    "hihat":  "percussive",
    "melody": "harmonic",
    "flux":   "full-spectrum",
}

# Pass-name tokens that imply real instrument detection.  Newly generated
# analysis must NOT serialize these in any public field.
RAW_INSTRUMENT_PASS_NAMES: frozenset[str] = frozenset({
    "kick", "snare", "hihat", "melody",
})

PUBLIC_LAYERS: tuple[str, ...] = ("percussive", "harmonic", "full-spectrum")

# Migration map for legacy-named passes in shipped analysis JSONs.  Renames
# the raw-instrument pass tokens to the equivalent broad-layer subpass IDs
# used by the current pipeline so public fields no longer expose
# kick/snare/hihat/melody/flux semantics while preserving each event's
# broad layer (percussive / harmonic / full-spectrum).  This is strictly
# a public-naming migration — broad layer membership, onset timings, and
# onset_class downstream classification are unchanged.
LEGACY_PASS_TO_BROAD_SUBPASS: dict[str, str] = {
    "kick":   "percussive_bass",
    "snare":  "percussive_broadband",
    "hihat":  "percussive_high_mid",
    "melody": "harmonic_low_mid",
    "flux":   "full_spectrum_flux",
}


def migrate_analysis_remove_raw_instrument_names(analysis: dict) -> dict:
    """Rewrite a loaded analysis dict so public fields use only broad-layer
    subpass IDs (no kick/snare/hihat/melody/flux).

    Touches three public surfaces:
      * ``events[*].passes`` — token list per event.
      * ``onsets`` — top-level per-pass onset summary keyed by pass name.
      * ``onset_diagnostics.raw_per_pass`` — pre-merge onset count per pass.

    The migration is idempotent: passes already using broad subpass IDs are
    left untouched.  ``events[*].layer`` is intentionally not modified —
    legacy JSONs already carry the correct broad layer field.
    """
    def _remap(name: str) -> str:
        return LEGACY_PASS_TO_BROAD_SUBPASS.get(name, name)

    # 1) events[*].passes
    for ev in analysis.get("events", []) or []:
        passes = ev.get("passes")
        if isinstance(passes, list):
            # Preserve order while deduplicating after the rename so two
            # legacy aliases that map to the same broad subpass collapse.
            seen: set[str] = set()
            renamed: list[str] = []
            for token in passes:
                token = _remap(token) if isinstance(token, str) else token
                if isinstance(token, str) and token in seen:
                    continue
                seen.add(token if isinstance(token, str) else repr(token))
                renamed.append(token)
            ev["passes"] = renamed
        legacy_pass = ev.get("pass")
        if isinstance(legacy_pass, str):
            ev["pass"] = _remap(legacy_pass)

    # 2) onsets — keep entries that remap to the same broad subpass merged
    #    (their timestamps coexist in the broad layer pool).
    onsets = analysis.get("onsets")
    if isinstance(onsets, dict):
        merged: dict[str, dict] = {}
        for name, summary in onsets.items():
            new_name = _remap(name) if isinstance(name, str) else name
            if not isinstance(summary, dict):
                merged[new_name] = summary
                continue
            if new_name not in merged:
                merged[new_name] = dict(summary)
                # Pass name now reflects broad subpass; method/zone retained.
                merged[new_name].pop("legacy_alias", None)
            else:
                target = merged[new_name]
                target["count"] = int(target.get("count", 0)) + int(summary.get("count", 0))
                ts_a = list(target.get("timestamps", []) or [])
                ts_b = list(summary.get("timestamps", []) or [])
                target["timestamps"] = sorted(set(round(float(t), 3) for t in (ts_a + ts_b)))
                res_a = list(target.get("resolutions", []) or [])
                res_b = list(summary.get("resolutions", []) or [])
                target["resolutions"] = res_a + res_b
        analysis["onsets"] = merged

    # 3) onset_diagnostics.raw_per_pass
    diag = analysis.get("onset_diagnostics")
    if isinstance(diag, dict):
        raw = diag.get("raw_per_pass")
        if isinstance(raw, dict):
            merged_counts: dict[str, int] = {}
            for name, count in raw.items():
                new_name = _remap(name) if isinstance(name, str) else name
                merged_counts[new_name] = merged_counts.get(new_name, 0) + int(count or 0)
            diag["raw_per_pass"] = merged_counts

    return analysis


def collapse_analysis_to_public_layers(analysis: dict) -> dict:
    """Rewrite public analysis surfaces to the three broad rhythm layers."""
    def _layer(name: str) -> str:
        if name in PUBLIC_LAYERS:
            return name
        return PASS_TO_LAYER.get(name, "full-spectrum")

    for ev in analysis.get("events", []) or []:
        layer = ev.get("layer")
        if not isinstance(layer, str) or layer not in PUBLIC_LAYERS:
            passes = ev.get("passes")
            layer = _layer(str(passes[0])) if isinstance(passes, list) and passes else "full-spectrum"
            ev["layer"] = layer
        ev["passes"] = [layer]

    onsets = analysis.get("onsets")
    if isinstance(onsets, dict):
        merged: dict[str, dict] = {
            layer: {"method": "public_layer", "zone": None, "count": 0, "timestamps": []}
            for layer in PUBLIC_LAYERS
        }
        for name, summary in onsets.items():
            layer = _layer(str(name))
            target = merged.setdefault(
                layer,
                {"method": "public_layer", "zone": None, "count": 0, "timestamps": []},
            )
            if isinstance(summary, dict):
                timestamps = summary.get("timestamps", [])
                if isinstance(timestamps, list):
                    target["timestamps"].extend(timestamps)
                target["count"] += int(summary.get("count", 0))
        for summary in merged.values():
            timestamps = sorted({round(float(t), 3) for t in summary.get("timestamps", [])})
            summary["timestamps"] = timestamps
            summary["count"] = len(timestamps)
        analysis["onsets"] = merged

    diag = analysis.get("onset_diagnostics")
    if isinstance(diag, dict):
        raw = diag.get("raw_per_pass")
        if isinstance(raw, dict):
            merged_counts = {layer: 0 for layer in PUBLIC_LAYERS}
            for name, count in raw.items():
                layer = _layer(str(name))
                merged_counts[layer] = merged_counts.get(layer, 0) + int(count or 0)
            diag["raw_per_pass"] = merged_counts

    return analysis


def _collect_pass_tokens(analysis: dict) -> list[tuple[str, str]]:
    """Return (location, token) pairs for every pass-name token that appears
    in public surfaces of an analysis dict.  Used by the write-time guard
    (issue #480) to detect raw-instrument tokens before serialization."""
    found: list[tuple[str, str]] = []
    for ev in analysis.get("events", []) or []:
        passes = ev.get("passes")
        if isinstance(passes, list):
            for tok in passes:
                if isinstance(tok, str):
                    found.append(("events[*].passes", tok))
        legacy_pass = ev.get("pass")
        if isinstance(legacy_pass, str):
            found.append(("events[*].pass", legacy_pass))
    onsets = analysis.get("onsets")
    if isinstance(onsets, dict):
        for name in onsets.keys():
            if isinstance(name, str):
                found.append(("onsets[*]", name))
    diag = analysis.get("onset_diagnostics")
    if isinstance(diag, dict):
        raw = diag.get("raw_per_pass")
        if isinstance(raw, dict):
            for name in raw.keys():
                if isinstance(name, str):
                    found.append(("onset_diagnostics.raw_per_pass[*]", name))
    return found


def assert_no_raw_instrument_passes(analysis: dict) -> None:
    """Write-time guard (issue #480, protects #419/#448 from regressing).

    Walks every public pass-name surface of an analysis dict and raises
    ValueError if any token in :data:`RAW_INSTRUMENT_PASS_NAMES`
    (kick/snare/hihat/melody) is present.  Public artifacts must use the
    broad-layer subpass IDs only; if a legacy analysis is loaded, callers
    must migrate it via :func:`migrate_analysis_remove_raw_instrument_names`
    before writing.
    """
    offenders: list[tuple[str, str]] = [
        (loc, tok)
        for (loc, tok) in _collect_pass_tokens(analysis)
        if tok in RAW_INSTRUMENT_PASS_NAMES
    ]
    if offenders:
        sample = ", ".join(f"{loc}={tok!r}" for loc, tok in offenders[:5])
        raise ValueError(
            "Refusing to write analysis with raw-instrument pass names "
            f"({len(offenders)} occurrence(s); first: {sample}).  Public "
            "artifacts must use broad-layer subpass IDs only — call "
            "migrate_analysis_remove_raw_instrument_names() first."
        )


def _event_layer(event: dict) -> str:
    """Return the broad layer class for a single pre-merge onset event."""
    return PASS_TO_LAYER.get(event.get("pass", ""), "full-spectrum")


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
    peak_pick_cfg: dict | None = None,
) -> list[float]:
    """Detect onset peaks in an onset envelope.

    peak_pick_cfg keys (all optional, fall back to built-in defaults):
        pre_max   int   frames before peak used for local-max check (default 3)
        post_max  int   frames after  peak used for local-max check (default 3)
        pre_avg   int   frames before peak used for local-mean check (default 5)
        post_avg  int   frames after  peak used for local-mean check (default 5)
        wait      int   min frames between peaks (default 1)
    The ``threshold`` argument maps to librosa.util.peak_pick ``delta``
    (min height above local mean).  Lower → more detections.
    """
    if onset_env.size == 0:
        return []
    cfg = peak_pick_cfg or {}
    pre_max = int(cfg.get("pre_max", 3))
    post_max = int(cfg.get("post_max", 3))
    pre_avg = int(cfg.get("pre_avg", 5))
    post_avg = int(cfg.get("post_avg", 5))
    wait = int(cfg.get("wait", 1))
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
    peak_pick_cfg: dict | None = None,
    zone_thresholds: dict | None = None,
    method: str | None = None,
) -> list[float]:
    """Detect onsets for one pass/zone/resolution combination.

    The ``method`` argument dispatches the envelope source:

      * ``"spectral_flux"`` — full STFT positive spectral flux via
        :func:`_spectral_flux_onset_envelope`.  This is the envelope used by
        the ``full_spectrum_flux`` pass and is genuinely independent of the
        mel-band path; it is NOT a duplicate of ``band_flux_full``.
      * ``"band_flux_*"`` (default) — librosa mel-band positive flux
        envelope, optionally restricted to a frequency ``zone``.

    zone_thresholds (optional): per-zone threshold overrides, e.g.
        {"bass": 0.10, "low_mid": 0.13, "high_mid": 0.15}
    These take precedence over the global ``threshold`` when the zone matches.
    """
    effective_threshold = threshold
    if zone and zone_thresholds and zone in zone_thresholds:
        effective_threshold = float(zone_thresholds[zone])

    n_fft = int(resolution["n_fft"])
    hop_length = int(resolution["hop_length"])

    if method == "spectral_flux":
        # Full-spectrum STFT flux: an envelope independent from the mel-band
        # path so the ``full_spectrum_flux`` pass produces a distinct musical
        # view from ``percussive_broadband`` (issue #491).
        onset_env = _spectral_flux_onset_envelope(
            y=y,
            hop_length=hop_length,
            n_fft=n_fft,
            reduce="mean",
        )
    else:
        mel_db = _mel_spectrogram_db(y, sr, n_fft=n_fft, hop_length=hop_length, n_mels=n_mels)
        onset_env = _flux_envelope(mel_db, zone)
    return _detect_onsets_from_envelope(
        onset_env,
        sr=sr,
        hop_length=hop_length,
        threshold=effective_threshold,
        peak_pick_cfg=peak_pick_cfg,
    )


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
    onset_env = np.asarray(onset_env, dtype=np.float32)
    positive = onset_env[onset_env > 0.0]
    if positive.size > 0:
        scale = float(np.max(positive))
        if scale > 0.0:
            onset_env = onset_env / scale
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
            # Short flip absorbed into previous run (keeps prev's is_quiet).
            prev = merged[-1]
            merged[-1] = (prev[0], run[1], prev[2])
        elif merged and merged[-1][2] == run[2]:
            # Smoothing can leave the next run with the same is_quiet as the
            # previous merged run (e.g. Q_long, N_short→absorbed into Q,
            # Q_long).  Coalesce so canonical alternating transitions remain.
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
    peak_pick_cfg = dict(onset_cfg.get("peak_pick", {}))
    zone_thresholds_cfg = {
        str(k): float(v)
        for k, v in onset_cfg.get("zone_thresholds", {}).items()
    }
    onsets: dict[str, list[float]] = {}
    onset_breakdown: dict[str, list[dict]] = {}
    resolutions = onset_resolutions(librosa_config)
    for p in ONSET_PASSES:
        combined: list[float] = []
        per_resolution: list[dict] = []
        # Determine effective threshold for this pass (zone-aware override).
        zone = p["zone"]
        eff_thresh = float(
            zone_thresholds_cfg.get(zone, onset_threshold)
            if zone
            else onset_threshold
        )
        for res in resolutions:
            detected = _detect_pass_onsets(
                y=y,
                sr=sr,
                zone=zone,
                threshold=onset_threshold,
                resolution=res,
                n_mels=n_mels,
                peak_pick_cfg=peak_pick_cfg,
                zone_thresholds=zone_thresholds_cfg,
                method=p.get("method"),
            )
            per_resolution.append(
                {
                    "n_fft": int(res["n_fft"]),
                    "hop_length": int(res["hop_length"]),
                    "count": len(detected),
                    "effective_threshold": round(eff_thresh, 4),
                }
            )
            combined.extend(detected)

        # De-duplicate exact-frame overlaps across resolutions.
        o = sorted({round(t, 6) for t in combined})
        onsets[p["name"]] = o
        onset_breakdown[p["name"]] = per_resolution
        zone_label = f"[{zone}]" if zone else "[full] "
        print(
            f"      {p['name']:8s} {zone_label:10s} ({p['method']:16s}): "
            f"{len(o)} onsets  thresh={eff_thresh:.3f}  "
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

    # Song duration = true audio track duration.  Older revisions inflated
    # this with `max(true_duration, last_event_time + 1.0)`; that lied about
    # the audio length and caused downstream timing/structure math to
    # operate past the actual file end (issue #477).  We always report the
    # real librosa-measured duration here.  Consumers that need the latest
    # event boundary read it from `events[-1].t` directly; the explicit
    # `last_event_time` field below preserves that data without overwriting
    # the audio duration.
    all_times = beats + [t for v in onsets.values() for t in v]
    last_event_time = max(all_times) if all_times else 0.0

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
        "duration": duration,
        "last_event_time": last_event_time,
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

    IMPORTANT — layer-class rule (directive 2026-05-10):
        Events from DIFFERENT broad layer classes (percussive / harmonic /
        full-spectrum) are NEVER merged, even when their timestamps fall within
        the merge window.  Only events that share the same layer class may be
        collapsed.  This prevents a percussive onset and a harmonic onset that
        are 40 ms apart from being silently fused into a single full-spectrum
        event, which would cause center-lane (square/lane-1) bias.

    Within the same layer class the original sliding-window logic applies:
        group anchor = first event in the class-sorted stream;
        all same-class events within merge_window of the anchor join the group.
    """
    if not events:
        return []

    # Separate pre-merge events by broad layer class.
    by_layer: dict[str, list[dict]] = {}
    for event in events:
        layer = _event_layer(event)
        by_layer.setdefault(layer, []).append(event)

    merged_all: list[dict] = []
    for layer, layer_events in by_layer.items():
        layer_events = sorted(layer_events, key=lambda e: e["t"])
        # First pass: anchor-window grouping (each anchor swallows events
        # whose timestamp is within merge_window of the anchor itself).
        first_pass: list[dict] = []
        i = 0
        while i < len(layer_events):
            group = [layer_events[i]]
            j = i + 1
            while (
                j < len(layer_events)
                and layer_events[j]["t"] - layer_events[i]["t"] < merge_window
            ):
                group.append(layer_events[j])
                j += 1

            representative = max(group, key=lambda e: (float(e.get("flux", 0.0)), -float(e["t"])))
            t_rep = round(float(representative["t"]), 3)
            passes = sorted({e["pass"] for e in group})
            max_flux = round(max(e["flux"] for e in group), 4)

            first_pass.append(
                {
                    "t": t_rep,
                    "flux": max_flux,
                    "passes": passes,
                    # 'layer' field allows level_designer to skip the passes-based
                    # inference and directly use the authoritative class.
                    "layer": layer,
                }
            )
            i = j

        # Second pass — same-layer adjacency floor (issue #467):
        # The first-pass representative timestamp is the *mean* of its
        # group, so two consecutive groups can still end up < merge_window
        # apart on the rep stream (e.g. anchor-window groups [0, 49ms] and
        # [70ms] yield reps 24.5ms and 70ms — only 45.5ms apart).  Sweep
        # the rep stream and merge any neighbours still inside the window.
        # Cross-layer events are unaffected: this loop is per-layer.
        if first_pass:
            collapsed: list[dict] = [first_pass[0]]
            for ev in first_pass[1:]:
                prev = collapsed[-1]
                if ev["t"] - prev["t"] < merge_window:
                    merged_passes = sorted(set(prev["passes"]) | set(ev["passes"]))
                    if ev["flux"] > prev["flux"] or (ev["flux"] == prev["flux"] and ev["t"] < prev["t"]):
                        prev["t"] = ev["t"]
                    prev["flux"] = round(max(prev["flux"], ev["flux"]), 4)
                    prev["passes"] = merged_passes
                else:
                    collapsed.append(ev)
            merged_all.extend(collapsed)
        # If layer had zero events nothing to extend.

    merged_all.sort(key=lambda e: (e["t"], e["layer"]))
    return merged_all


# ---------------------------------------------------------------------------
# STEP 4 - CLASSIFY BY INTENSITY (mel band energy -> easy/medium/hard filter)
# ---------------------------------------------------------------------------

def classify_intensity(
    events: list[dict],
    quiet_regions: list[dict],
    duration: float | None = None,
) -> list[dict]:
    """
    Tag each event with intensity (low / medium / high) based on:
    - Whether it falls in a quiet region (RMS-derived quiet output)
    - Its flux score relative to the overall distribution

    A trailing QUIET marker with no closing NOISY transition is treated as
    a quiet window that runs through the end of the analysis.  ``duration``
    may be passed explicitly; otherwise a sentinel that exceeds any event
    timestamp is used.
    """
    # build quiet time windows: a QUIET region runs from a QUIET marker
    # to the next NOISY marker
    quiet_windows = []
    quiet_start = None
    for r in quiet_regions:
        if r["type"] == "QUIET":
            if quiet_start is None:
                quiet_start = r["t"]
        elif r["type"] == "NOISY" and quiet_start is not None:
            quiet_windows.append((quiet_start, r["t"]))
            quiet_start = None
    if quiet_start is not None:
        if duration is None:
            event_max = max((e["t"] for e in events), default=quiet_start)
            close = max(event_max, quiet_start) + 1.0
        else:
            close = max(float(duration), quiet_start)
        quiet_windows.append((quiet_start, close))

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

    # ── Onset pool diagnostics (computed before merge) ───────────────────────
    raw_total = sum(len(features["onsets"][p["name"]]) for p in ONSET_PASSES)
    duration_sec = max(1.0, features["duration"])
    print(f"    raw onsets total (all passes, pre-merge): {raw_total}")
    print(f"    per-pass/zone breakdown:")
    per_pass_raw: dict[str, int] = {}
    for p in ONSET_PASSES:
        n = len(features["onsets"][p["name"]])
        per_pass_raw[p["name"]] = n
        zone_label = f"[{p['zone']}]" if p["zone"] else "[full] "
        print(f"      {p['name']:8s} {zone_label:10s}: {n}")
    total_raw_events_pre_merge = len(events)
    print(f"    flat events (pre-merge): {total_raw_events_pre_merge}")

    print("[3] Merging overlapping events (window=50ms, same-layer-class only)...")
    events = merge_events(events)
    events_per_min = round(len(events) / duration_sec * 60.0, 1)
    print(f"    after merge: {len(events)} unique events  ({events_per_min}/min)")
    print(f"    (cross-layer events preserved separately even when < 50ms apart)")

    print("[4] Classifying intensity from quiet regions...")
    events = classify_intensity(events, features["quiet"], duration=features.get("duration"))

    print("[5] Building song structure...")
    structure = build_structure(features)

    # ── Segment coverage (events per structure section) ───────────────────────
    seg_event_counts: list[dict] = []
    empty_segments = 0
    for sec in structure:
        t0, t1 = float(sec["start"]), float(sec["end"])
        n_in = sum(1 for e in events if t0 <= e["t"] < t1)
        empty = n_in == 0
        if empty:
            empty_segments += 1
        seg_event_counts.append({
            "section": sec.get("section", "?"),
            "start": t0,
            "end": t1,
            "event_count": n_in,
            "empty": empty,
        })
    print(f"    structure segments: {len(structure)}  "
          f"(empty={empty_segments}, "
          f"coverage={len(structure)-empty_segments}/{len(structure)})")

    pass_summary = {
        layer: {"method": "public_layer", "zone": None, "count": 0, "timestamps": []}
        for layer in PUBLIC_LAYERS
    }
    for p in ONSET_PASSES:
        layer = PASS_TO_LAYER[p["name"]]
        pass_summary[layer]["timestamps"].extend(
            round(t, 3) for t in features["onsets"][p["name"]]
        )
    for summary in pass_summary.values():
        timestamps = sorted(set(summary["timestamps"]))
        summary["timestamps"] = timestamps
        summary["count"] = len(timestamps)

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
    print(f"\n    event summary: {len(events)} events  ({events_per_min}/min)")
    for k, v in sorted(intensity_counts.items()):
        print(f"      {k:8s}: {v}")
    print(f"    beats: {len(features['beats'])}  "
          f"BPM: {round(features['bpm'], 1)}  "
          f"duration: {round(features['duration'], 1)}s")
    print(f"    segment coverage: {len(structure) - empty_segments}/{len(structure)} "
          f"({empty_segments} empty)")

    # ── Structured onset diagnostics stored in JSON for comparison ───────────
    onset_diagnostics = {
        "raw_per_pass": {
            layer: sum(count for name, count in per_pass_raw.items() if PASS_TO_LAYER[name] == layer)
            for layer in PUBLIC_LAYERS
        },
        "raw_total": raw_total,
        "flat_events_pre_merge": total_raw_events_pre_merge,
        "merged_events": len(events),
        "events_per_minute": events_per_min,
        "beats_total": len(features["beats"]),
        "bpm": round(features["bpm"], 2),
        "duration_sec": round(features["duration"], 2),
        "segment_count": len(structure),
        "empty_segment_count": empty_segments,
        "segment_coverage": [
            {k: v for k, v in seg.items()} for seg in seg_event_counts
        ],
    }

    analysis = {
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
        "onset_diagnostics": onset_diagnostics,
    }
    return collapse_analysis_to_public_layers(analysis)


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
    # Issue #480 — write-time guard: refuse to serialize raw-instrument
    # pass names so #419/#448 cannot recur silently.  build_analysis
    # already emits broad-layer subpass IDs, but this assertion is the
    # last line of defence against future regressions in any code path
    # that reaches this writer.
    assert_no_raw_instrument_passes(analysis)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(analysis, f, indent=2)
    print(f"\n✓ {out_path}  ({len(analysis['events'])} events)")
    print("=" * 60)


if __name__ == "__main__":
    main()
