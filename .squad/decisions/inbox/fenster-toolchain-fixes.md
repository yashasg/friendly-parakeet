# 2026-05-10 — Toolchain fixes decisions (issues #382-#385)

## Decision 1: Count gates must use the same filtered population as metrics
Loop-2 content validation now compares declared `count` to valid integer-beat rows, not raw array length. We still expose `raw_beat_rows` for diagnostics so malformed rows are visible instead of hidden.

## Decision 2: Diagnostics outputs are treated as mode-specific reproducible artifacts
`write_snap_diagnostics()` now removes known generated artifacts before each run in a target directory. This prevents stale experimental CSV files from being paired with non-experimental summaries.

## Decision 3: Onset spike validator requires experimental-mode summary
`validate_onset_spike_artifacts.py` now exits with an error if `experimental_onset_timing.enabled` is not true, rather than attempting to consume potentially stale/mismatched files.

## Decision 4: Segment-focus event identity must not collapse by beat index
Segment-focus selection now keys events by `(beat_idx, onset_class, source_event_idx)` and downstream timing joins on `source_event_idx`. This preserves layer-specific same-beat events end-to-end in onset-only generation.
