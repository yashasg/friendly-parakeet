# Fenster — History (SUMMARIZED)

**Last Updated:** 2026-05-10  
**Current Focus:** Beatmap quality diagnostics Loop 1 (shipped)  
**Status:** Diagnostics-only instrumentation in tools/level_designer.py via CLI flags

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Tools Engineer
- **Joined:** 2026-04-26T02:07:46.545Z

## 2026-05-10 — Onset extraction tuning + diagnostics

**Task**: Improve onset extraction sensitivity for sparse beatmaps (especially `1_stomper`).

### Root cause found
`_detect_onsets_from_envelope` had `wait=2` hardcoded (was conserving but slightly tight) and most critically,
`threshold=0.3` (peak_pick `delta`) on a 0→1 normalized envelope was far too aggressive.
The finetuned config had reverted to a single resolution (`n_fft=2048, hop_length=384`) instead of 3,
losing 2/3 of detection surface. Combined: only 33 events from 5 onsets/pass for a 200s song.

### Changes made
1. **`rhythm_pipeline.py`** — `_detect_onsets_from_envelope` and `_detect_pass_onsets`:
   - Peak-pick params (`pre_max`, `post_max`, `pre_avg`, `post_avg`, `wait`) now read from `peak_pick` config section.
   - Zone-specific threshold overrides (`zone_thresholds`) now applied per-pass before `peak_pick`.
   - `build_analysis` emits `onset_diagnostics` key with raw_per_pass, merged_events, events_per_minute, segment_coverage.

2. **Config files** (both `rhythm_librosa_params.json` and `rhythm_librosa_finetuned_params.json`):
   - `threshold`: 0.30 → 0.15 (global)
   - `zone_thresholds`: `{bass: 0.10, low_mid: 0.13, high_mid: 0.15}`
   - `peak_pick`: `{wait: 1}` (was hardcoded 2), rest default 3/3/5/5
   - Finetuned: restored from 1 resolution to 3 (`n_fft: 1024/2048/4096, hop_length: 256`)

3. **`level_designer.py`** — `write_snap_diagnostics`:
   - `onset_pool_summary` now always emitted (not gated on experimental flag).
   - Fields: `total_events`, `events_per_minute`, `snapped_events`, `snap_residual_stats`,
     `segment_count`, `empty_segment_count`, `segment_coverage`, `raw_per_pass`, `raw_total`.
   - Experimental block now includes `obstacle_counts_by_difficulty`.

4. **Tests** (`test_level_designer_onset_spike.py`) — 6 new tests added:
   - `test_onset_pool_summary_present_in_diagnostics`
   - `test_onset_pool_summary_experimental_adds_obstacle_counts`
   - `test_build_beatmap_counts_scale_with_difficulty`
   - `test_richer_onset_pool_in_varied_fixture`
   - `test_segment_diagnostics_include_n_difficulty_fields`
   - `test_diagnostics_deterministic_across_runs`

### Results
| Song | Events before | Events after | BPM |
|---|---|---|---|
| 1_stomper | 33 (5/pass) | 204 (61/min) | 160 |
| 2_drama | (regenerated) | 1085 (266/min) | 130 |
| 3_mental_corruption | (regenerated) | 1736 (460/min) | 150 |

**1_stomper obstacles**: 14/17/19 → **23/35/51** (easy/medium/hard)

All 17 Python tests pass. All 1848 C++ assertions pass.

## 2026-05-10 — Tools/Beatmap Audit (Issues #382-#385)

Audit scope: `tools/level_designer.py`, rhythm/beatmap validators, diagnostics generation, test tools, run scripts, and the onset beatmap generation workflow. No code changes were made in this pass beyond this history note.

Findings filed:

1. **#385 — Onset segment-focus path collapses layer-specific events sharing a beat**
   - `snap_events_to_beats()` preserves one event per `(beat_idx, onset_class)`, but the active segment-focus path rekeys selected events by `beat_idx` only.
   - Learning: layer-aware preservation must be verified end-to-end, not only at merge/snap boundaries. Any necessary same-beat collision policy should be explicit and diagnosed.

2. **#384 — Diagnostics output can leave stale onset spike artifacts across modes**
   - `write_snap_diagnostics()` uses `exist_ok=True` and overwrites the summary, but only writes `onset_timing_events.csv` in experimental mode.
   - Learning: diagnostics directories should be reproducible run outputs. Generated artifacts need cleanup, a manifest, or mode-specific output directories to prevent stale-file validation.

3. **#382 — run.sh ignores caller-provided VCPKG_ROOT**
   - `run.sh` unconditionally exports `VCPKG_ROOT="$HOME/vcpkg"` before calling `build.sh`, defeating environment-based toolchain configuration.
   - Learning: wrapper scripts should preserve caller-provided tool paths and let lower-level scripts validate them.

4. **#383 — Loop 2 content validator count check ignores invalid beat rows**
   - The validator compares declared `count` to raw `len(beats)` while the rest of the metrics use filtered integer-beat rows.
   - Learning: validator diagnostics should distinguish raw rows, valid authored obstacles, and declared counts so malformed rows cannot hide behind matching totals.

Duplicate checks were run with `gh issue list --state all --search` for each finding before filing. Labels applied: `squad`, `squad:fenster`.

## 2026-05-10 — Fixed audit issues #382, #383, #384, #385

Implemented focused fixes on branch `audit/autonomous-quality-loop`.

- **#382** (`run.sh`): preserved caller-provided toolchain path by defaulting only when unset:
  - `export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"`

- **#383** (`tools/validate_loop2_content_gates.py`): count gate now compares declared `count` against valid integer-beat obstacles (same population used by other metrics), and diagnostics now report all three numbers: declared count, valid beat count, and raw rows.
  - Added test coverage in `tools/test_validate_loop2_content_gates.py` for invalid beat rows.

- **#384** (`tools/level_designer.py`, `tools/validate_onset_spike_artifacts.py`): diagnostics writer now clears known generated artifacts in output dir before writing, preventing stale `onset_timing_events.csv` carryover across mode switches. Spike validator now hard-errors when summary is not `experimental_onset_timing.enabled=true`.
  - Added tests for stale-artifact cleanup and non-experimental validator failure.

- **#385** (`tools/level_designer.py`): segment-focus selection now keys selected/snapped events by `(beat_idx, onset_class, source_event_idx)` and preserves per-event onset class instead of collapsing by beat index. Timing attachment/diagnostics lookups now resolve events by `source_event_idx` (with safe beat fallback only when unambiguous).
  - Added focused test proving same-beat cross-layer events remain distinct when selected.

Validation run:
- Targeted tests:
  - `python3 -m unittest tools/test_validate_loop2_content_gates.py tools/test_level_designer_onset_spike.py tools/test_validate_onset_spike_artifacts.py` ✅
- Full validation:
  - `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests` ✅

## 2026-05-10T02:40:52-07:00 — Autonomous Quality Loop 2 Audit (Post-PR #408)

Filed follow-up tooling audit issues:
- #409 Validators crash on malformed/non-integer beat rows (`validate_max_beat_gap.py`, `validate_gap_one_readability.py`) instead of reporting diagnostics.
- #410 `validate_beatmap_offset.py` is cwd-dependent (`Path("content/beatmaps")`) and fails outside repo root.

Learning: After hardening one validator path (#383), audit sibling validators for the same malformed-row and path-resolution assumptions; these regressions tend to exist in parallel scripts.

## 2026-05-10T15:45:30-07:00 — Issue #125 Revision: Docs Cleanup

Reviewer-assigned revision owner for documentation audit (issue #125 closure consistency).

- **Artifact:** `design-docs/feature-specs.md`
- **Change:** Removed `LOW_BAR_BASE_PTS` and `HIGH_BAR_BASE_PTS` rows from active "Balancing Parameters" table (lines 741–742). These constants are no longer part of live runtime architecture and were incorrectly presented as active balancing guidance.
- **Validation:**
  - `git diff --check` ✅ (no whitespace issues)
  - Grep search: no remaining references in active docs ✅

Minimal docs-only fix per reviewer feedback. Not committed pending further review.
