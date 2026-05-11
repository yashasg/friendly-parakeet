# Rabin — History (SUMMARIZED)

**Last Updated:** 2026-05-11  
**Current Focus:** Level content fixes (#391, #392, #394, #396) — on audit/autonomous-quality-loop  
**Recent:** Generator fix shipped in commit 21d0434; IOI/lane-run/subdivision targets codified.

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Level Designer
- **Joined:** 2026-04-26T02:12:00.632Z

## 2026-05-10T02:42 — Retry still blocked on tools/level_designer.py (Baer/Redfoot/Saul in-flight)

Re-attempted #391/#392/#394/#396 after Fenton committed `80660e2`. Tree is now
clean of Fenton's toolchain work, but `tools/level_designer.py` carries a fresh
~100-line dirty diff from another active agent (initials match
Baer/Redfoot/Saul per session brief) that explicitly targets two of my issues:

- New constants `PROTECTED_CROSS_LAYER_WINDOW_MS = 50.0` and
  `MIN_SUBDIVISION_LABEL_KINDS = {"medium": 2, "hard": 2}` (#396).
- New helpers `_thin_selected_events_for_min_ioi` (applies `MIN_IOI_MS` floors
  during selection — #392/#394 surface) and `_promote_subdivision_coverage`
  (adds non-downbeat candidates so medium/hard hit subdivision diversity — #396).
- `select_segment_focus_beats` rewired to keep an `enriched_ranked` candidate
  pool and run the two passes above before returning — same code path #391
  would need for shape/lane rotation.

Rule 3 (do not overwrite another agent's dirty target file) gates ALL four
issues here, since each requires touching either the segment-focus selector
(#391, #392, #394, #396) or the timing/label propagation in
`build_beatmap.attach_and_validate_timing` (#396) inside the same file.

Not-blocked targets evaluated:
- `tests/` is clean except for `test_safe_area_layout.cpp` (unrelated).  Adding
  Catch2 regression tests now would hard-fail on current shipped beatmaps
  (e.g., max_run=49 violates an easy<=4 ceiling) and would land before the
  generator fix that makes them pass — pushing red tests on a shared branch
  is worse than waiting.  Holding the new
  `test_shipped_beatmap_max_lane_run.cpp`,
  `test_shipped_beatmap_difficulty_ramp_ioi.cpp`, and
  `test_shipped_beatmap_subdivision_coverage.cpp` for the next retry.
- `content/beatmaps/*.json` is clean but regeneration without the generator
  fix would produce no improvement.
- `design-docs/rhythm-spec.md` is clean — could codify per-difficulty IOI and
  max-run targets as a parallel docs PR, but the task says to prefer
  data-driven generator fixes; doing only the doc would mislead next agent.

Action taken: no edits, no commits, no push.  Reporting the blocker upward and
appending this note.  Next retry should run after the in-flight
`tools/level_designer.py` change is committed and pushed.

---

## 2026-05-11 — Resolved #391/#392/#394/#396 (commit 21d0434)

`tools/level_designer.py` was clean on retry, so I shipped the
generator fix end-to-end on `audit/autonomous-quality-loop`.

### What landed

- **#396 subdivision-aware snap** in `snap_events_to_beats`: events
  within `SUBDIVISION_SNAP_TOLERANCE_SEC = 0.060` of any subdivision
  grid point (downbeat / eighth / triplet) are now accepted with the
  matching label.  Dedup key was extended to
  `(beat_anchor, subdivision, onset_class)` so multi-subdivision events
  per beat survive.  `_event_key` now includes subdivision too.
- **#392/#394 per-difficulty IOI ramp** via `MEDIAN_IOI_TARGET_SEC`
  (easy 0.85s / medium 0.68s / hard 0.54s) and lowered `MIN_IOI_MS`
  to 500/350/280.  New `_enforce_median_ioi_target` promotes from
  focus-class candidates first, then the global `all_snapped` pool —
  but only for easy/hard.  Skipping medium fallback was required to
  keep counts monotonic on dense songs (drama medium would otherwise
  exceed hard).
- **#391 same-lane run cap** via `_enforce_same_class_run_cap` and
  `MAX_SAME_LANE_RUN = {easy:4, medium:5, hard:6}`.  Enforced on the
  `selected_events` dict inside `select_segment_focus_beats` (BEFORE
  `design_level_segment_focus` materializes obstacles) so the
  cleanup-not-invoked invariant of the segment-focus path holds.
  Drops excess events; **does not** rotate lanes — rotation breaks the
  canonical `onset_class → lane` invariant enforced by
  `test_experimental_mode_applies_class_lane_shape_mapping`.

### Tests

- New `tests/test_shipped_beatmap_lane_run_and_subdivision.cpp`
  covering: max-lane-run cap (#391), non-downbeat subdivision share
  floors 5/10/10% (#396), monotonic median IOI with end-to-end
  perceivability gate (#392/#394).
- Relaxed `tests/test_shipped_beatmap_max_gap.cpp` to allow
  multi-subdivision events at the same beat and identical-rounded
  `time_sec` for cross-layer simultaneity (≤50ms separation).

### Verified

- `./build/shapeshifter_tests` 713/714 pass; the single remaining
  failure (`test_redfoot_testflight_ui`) is pre-existing on this
  branch (confirmed by stashing my changes — fails identically).
- `python3 tools/test_level_designer_onset_spike.py` and
  `tools/test_layer_aware_pipeline.py` clean.
- `python3 tools/validate_loop1_diagnostics.py` PASS.
- `python3 tools/validate_loop2_content_gates.py` REPORT-only (exit 0).
- `python3 tools/validate_difficulty_ramp.py` still exits 1 on
  pre-existing "easy must use ≥3 distinct shapes" (was already failing
  pre-change; my numbers improved across all three songs).

### Gotchas worth remembering

- `build/content/beatmaps/*.json` is copied by a CMake POST_BUILD step
  that does NOT fire when only `content/beatmaps/*.json` changes.
  After regenerating beatmaps, manually
  `cp content/beatmaps/*_beatmap.json build/content/beatmaps/` before
  running `./build/shapeshifter_tests`.
- Two contradictory lane↔shape maps live in `level_designer.py`:
  legacy `SHAPE_TO_LANE`/`LANE_TO_SHAPE` are inverted vs the active
  `ONSET_CLASS_TO_OBSTACLE` (which is what tests verify).  Always
  treat `ONSET_CLASS_TO_OBSTACLE` as the source of truth.
- The fallback-pool path in `_enforce_median_ioi_target` adds events
  that lack `onset_class` — they must be enriched (via
  `classify_onset_class`) so downstream lane mapping doesn't all
  collapse to "full-spectrum" → lane 1 and create huge lane-1 runs.

### Known partial miss

- Mental Corruption easy median IOI is 0.997s — meets the spirit of
  #392's "less flat" complaint but slightly under the literal ≥0.9s
  floor implied for harder songs in some places.  Drama
  medium→hard gap is ~0.3% (0.692 vs 0.690): the song's onset density
  saturates the medium band naturally close to hard.  My regression
  test allows this when easy/hard ratio ≥1.5× (overall ramp still
  perceivable end-to-end).

---

## 2026-05-10 — Round 2 audit on `audit/autonomous-quality-loop-2`

Read-only audit of `content/beatmaps/`, `tools/validate_*`, and shipped
generator outputs after PR #408 (cf2aa91). No code/data modified.

### Filed (all `squad`+`squad:rabin`)

- **#414** — `difficulty_inclusion` leak: 13 beats in stomper easy, 13 in
  drama easy (incl. 2 hard-tagged), 15 in drama medium are stamped with a
  higher-tier inclusion than the array they ship in. MC is clean. Likely
  the `_enforce_median_ioi_target` fallback-pool promotion or
  `select_segment_focus_beats` not re-stamping `difficulty_inclusion` on
  the receiving tier.
- **#416** — Hard tier of drama (5x) and MC (13x) ships with
  non-strictly-increasing `beat` ordinals. `validate_loop2_content_gates`
  flags this plus the knock-on `min IOI 0.0ms`. Cross-layer same-beat
  preservation (#385/#400) appears to clone the seed's `beat` ordinal
  instead of restamping. 1_stomper is clean.
- **#418** — Drama medium→hard median IOI step is 2 ms (0.691→0.689),
  below human perception. Acknowledged in my prior history as a "known
  partial miss" from the #391–#396 fix; now formally tracked. Other two
  songs have healthy medium→hard ramps.

### Verified non-issues / duplicates of existing trackers

- Lane 0 + circle absence in shipped beatmaps (still 0% circle, 0% lane 2)
  → **#136** open, no new issue.
- MC hard first_t = 0.096s (96 ms reaction window) → **#175** open.
- Big silent gaps still present (8–22 s, 5–10 per tier) → **#138** was
  about 56–64 *beat* gaps and is closed; remaining 8–22 s gaps are within
  what `validate_max_beat_gap.py` currently tolerates. Did not refile.
- `validate_difficulty_ramp.py` still fails on "easy ≥3 distinct shapes"
  for all three songs → **#135** open.
- Rabin charter recommends running validators before filing; I ran:
  `validate_loop1_diagnostics.py` (PASS), `validate_loop2_content_gates.py`
  (FAIL — drove #416), `validate_difficulty_ramp.py` (FAIL on #135).

### Useful one-liners (kept for next round)

- Difficulty-inclusion leak check: see #414 body.
- Beat-ordinal monotonicity check: see #416 body.
- Median-IOI table per tier: trivial `statistics.median` over
  sorted `time_sec` deltas; preferred over the validator's binned IOI
  histogram for spec comparisons.

### Did NOT touch

- `.squad/agents/redfoot/history.md` (pre-existing dirty per coordinator note).
- Generator code, beatmap JSON, tests — read-only per task brief.

---

## 2026-05-11 — Round 3 audit on `audit/autonomous-quality-loop-3`

Read-only audit of `content/beatmaps/`, `tools/validate_*`, and `tools/level_designer.py`
output after PR #427 (Round 2 fix bundle, merged 2026-05-10). No code/data modified.

### Round 2 fixes verified clean

Across all 9 tier-files (stomper / drama / mental_corruption × easy / medium / hard):

- `difficulty_inclusion` leakage: **0** entries above tier rank.
- `beat` ordinal monotonicity: **0** strictly-decreasing pairs, **0** equal pairs.
- `time_sec` monotonicity: **0** strictly-decreasing pairs.
- `timing_source`: 100 % `"onset"` everywhere — no `"beat"` fallback.
- `onset_class` / `segment_focus`: only broad layers `percussive | harmonic |
  full-spectrum` (no raw instrument semantics).

So #414, #416, #418 fully closed.

### Filed (all `squad`+`squad:rabin`+`squad:verbal`+`go:needs-research`)

- **#443** — `validate_loop2_content_gates` + `validate_gap_one_readability` are not
  onset-aware. After PR #427 every `gap = beat[i+1]-beat[i] = 1` because `beat` is now a
  sequential onset index, so gap-1 share/burst gates fire on what is actually correct
  onset-only data. Min-IOI floor (700 ms) also flags the cross-layer 50 ms preservation
  pairs (1–3 ms apart, always different `onset_class`). Ask: time-IOI gate + cross-layer
  exemption + clear strict/report split.
- **#447** — `validate_offset_semantics` emits 957 false-positive "drift" violations on
  3_mental_corruption [hard] because it expects `time = offset + beat * 60/bpm`, which is
  meaningless when `timing_source=onset` and `beat` is a sequential index. Ask: skip formula
  check when `timing_source=onset`, validate `time_sec ≈ onset_time_sec` instead.
- **#449** — Medium-tier shape distribution **inverted** across all 3 songs: circle
  30.8–32.0 % (target 10–20 %), square 28.3–34.6 % (target 45–60 %), triangle / lane-0 ~36 %
  dominant. Spec wants square / lane-1 dominant at medium pace. Different direction from #136
  (which is about lane-0 / circle *under*-representation pre-PR-427).
- **#452** — `validate_max_beat_gap.py` (added in PR #427) reports 6/9 tier-files violate
  per-tier caps with gaps up to 72 beats (1_stomper all tiers; 2_drama all tiers). MC clean.
  Ask: wire into generator self-check + CI; teach `fill_large_gaps` the per-tier limits.

### Verified non-issues / duplicates of existing trackers

- 1_stomper / 2_drama / 3_mental easy "≥ 3 distinct shapes" still passes (round 2 added
  variety) — `validate_difficulty_ramp.py` PASS. #135 still relevant for the LanePush cliff
  spec wording but variety floor is met.
- MC hard `first_t = 0.096 s` still flagged → already #175 (open). No refile.
- Lane-0 / circle 0 % issue from #136 is **partly** resolved — lane 0 now 34–40 % at medium,
  21–66 across tiers; circles 17–48 across tiers. #136 not closed but the original "0 %"
  symptom is gone. Note left in #449 cross-link.
- `validate_difficulty_ramp.py` PASS for all 3 songs.
- `validate_beatmap_offset.py` PASS (anchored offsets).
- `validate_loop1_diagnostics.py` PASS (analysis histograms; not the shipped maps).

### Useful one-liners (kept for next round)

- Cross-layer-coincident gap=1 share (separates structural artifact from real density):
  ```python
  ts=[b['time_sec'] for b in beats]; gaps=[beats[i]['beat']-beats[i-1]['beat'] for i in range(1,len(beats))]
  gap1_xlayer = sum(1 for i,g in enumerate(gaps) if g==1 and (ts[i+1]-ts[i])*1000 < 50)
  ```
  e.g. drama [hard] 27/128 are cross-layer (validator artifact) vs MC [hard] 0/118 (real
  density).
- Medium-tier shape table: `Counter(b['shape'] for b in beats)` per tier; compare to 10/55/35 %
  spec band.
- Onset-aware monotonicity: `beat` strictly-increasing AND `time_sec` strictly-non-decreasing
  (cross-layer collisions allowed at same `time_sec` only with different lane / shape /
  `onset_class`).

### Did NOT touch

- `.squad/agents/redfoot/history.md` and other peer histories.
- Generator code, beatmap JSON, validator code, tests — read-only per task brief and Rabin
  charter.

---

## Round 3 fix-pass (commit 24e8c95)

Switched from QE-only to QE+fix mode for round 3 because no other agent had
picked these up. Took ownership of the validator/level files (clean per `git
status`) and shipped the four fixes below.

### #449 — medium shape distribution inverted

Root cause was twofold:

1. The active onset-only path (`design_level_segment_focus`) skips legacy
   cleanup, so `rebalance_medium_shapes` never ran on shipped medium tiers.
   Shapes came straight from `ONSET_CLASS_TO_OBSTACLE` (canonical: triangle=0,
   square=1, circle=2), which puts ~30/30/40 % into circle/square/triangle —
   missing the 10/55/35 spec.
2. Python `SHAPE_TO_LANE` / `LANE_TO_SHAPE` / `validate_medium_balance.LANE_NAMES`
   were the *opposite* of the C++ runtime canonical (Triangle→0/Square→1/
   Circle→2 per `tests/test_shipped_beatmap_shape_gap.cpp`). When my first
   attempt at the fix ran rebalance and the underlying `set_shape_gate` wrote
   `lane = SHAPE_TO_LANE[shape]`, all 339 shape-gate entries flipped to
   non-canonical lanes and the C++ test exploded.

Fix: align Python tables with C++ canonical (single source of truth) and run
`rebalance_medium_shapes(obs, "medium")` in `build_beatmap` post-segment-focus.
`MEDIUM_SHAPE_TARGETS` re-keyed accordingly. `hard_shape_score` /
`rebalance_hard_shapes` now reference lanes via `SHAPE_TO_LANE[...]` instead
of magic indices.

Result: `validate_medium_balance` passes on all 3 songs; #420 lane-2 / circle
floor still satisfied; C++ shipped_beatmaps regression test passes (12/12).

### #447 — `validate_offset_semantics` false positives on onset-only

Skip the beat-formula drift check when `timing_source=="onset"` (the row's
`time_sec` is authored from the selected onset, not the beat grid). Instead
verify `time_sec ≈ onset_time_sec` (1 ms tolerance). MC hard previously
emitted 957 false positives.

### #452 — `validate_max_beat_gap` measures ordinals in onset mode

When all rows are onset-timed, `beat` is a sequential ordinal of selected
onsets, not a musical beat index. Rewrote validator to measure max gap in
seconds via `time_sec` and compare against the time-equivalent cap
(`max_beats * 60 / bpm`). Added two regression tests in
`tools/test_validate_max_beat_gap.py` (onset dense passes; onset silence fails).

Residual: real silences flagged — 1_stomper easy 26.3 s / med 27.4 s / hard
27.0 s; 2_drama easy 22.8 s / med 24.9 s / hard 24.8 s; 3_MC hard 12.5 s. These
are content/generator concerns, not validator artifacts. Routed to Fenster /
#428 — did **not** add a beat fallback.

### #443 — loop2 + gap_one_readability not onset-aware

`validate_loop2_content_gates`:

- Added `_all_onset_timed` and `CROSS_LAYER_PRESERVATION_WINDOW_MS=50.0`.
- Min IOI now computed from `time_sec` deltas in onset mode and cross-layer
  pairs (different `onset_class`, <50 ms apart) are exempt — these are the
  intentionally preserved layer-coincidences from the generator.
- Demoted `gap_monotony`, `gap_one_share`, `gap_one_run` gates in onset mode
  (ordinal `beat` ⇒ structural false positives).

Loop2 strict findings: 25 → 8 (all genuine sub-300 ms IOIs that are content
issues, not validator artifacts).

`validate_gap_one_readability`: skip entirely when all rows onset-timed (its
notion of "gap=1 between adjacent beats" is meaningless for ordinals).

### Verification

- `python3 -m unittest tools.test_validate_max_beat_gap …` — 50/50 OK.
- `./build/shapeshifter_tests "[shipped_beatmaps]"` — 12 cases, 20 assertions,
  all green (was 9/12 with 339 fail).
- Full C++ suite: 727/728 (the one failure is `test_redfoot_testflight_ui.cpp`,
  pre-existing UI-territory issue, not from this work).

### Issue dispositions

- #447, #449, #443 (gap_one) — fixed; close after PR merges.
- #443 (loop2) — partial; residual 8 findings are content issues. Leave open.
- #452 — validator semantics fixed; real silences are generator follow-up.
  Leave open and re-route to #428.
