# Rabin — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Level Designer
- **Joined:** 2026-04-26T02:12:00.632Z

## 2026 — Issue #137: rhythm_pipeline offset = beats[0] timing audit

**Question:** Are shipped beatmap `offset` values musically correct, or do
they cause off-beat collisions?

**Method:** For each shipped beatmap × difficulty, compute the predicted
collision time `t_pred = offset + beat_index * (60/bpm)` (the formula used
by `beat_scheduler_system.cpp:27`) and compare against the analyzed onset
`analysis.beats[beat_index]`.

**Evidence:**
- `offset` in all 3 shipped beatmaps equals `analysis.beats[0]` exactly
  (stomper 2.27, drama 1.813, mental_corruption 0.418).
- First-obstacle predicted time matches analysis onset within ±1 ms across
  all 9 difficulty maps.
- Max drift across **every** scheduled obstacle (uniform-spaced predicted vs
  analysis beat time): stomper 0.7 ms, drama 0.0 ms, mental_corruption 0.5 ms.
  Aubio's tempo tracker is producing essentially-uniform beat grids, so the
  uniform `offset + i*period` model used by the scheduler is faithful.

**Conclusion:** Shipped beatmaps are timing-correct under the current
scheduler semantics. The "off-beat collision" risk in #137 is a *schema-
semantics* concern (does `offset` mean "first audible beat" or "intended
first collision"?), not a content defect. Fenster owns the semantic
definition and any pipeline/scheduler code change.

**Action:** No content regeneration. If Fenster lands a semantics change
(e.g. offset = beats[first_collision_beat] + relative beat indices), regen
all 3 beatmaps from existing `*_analysis.json` and re-run
`check_shape_change_gap.py`, `check_bar_coverage.py`,
`validate_difficulty_ramp.py`, and the C++ test suite. No rhythm_pipeline
rerun needed (analysis JSON unchanged).

**Content gates preserved:** #125 bar coverage, #134 shape-gap, #135
difficulty ramp untouched.

## 2026-04-27 — Issue #137 Complete: Team Outcome

- **Status:** ✅ APPROVED. Issue #137 offset semantics work completed with team.
- **Content audit outcome:** All 3 shipped beatmaps confirmed timing-correct within ±1ms to analyzed onsets. No independent content defect.
- **Regeneration:** Beatmaps regenerated under Fenster's new anchor semantics. Stomper offset 2.270→2.269s (1ms); drama/mental unchanged. All gates re-verified.
- **Contingency:** If Fenster changes semantics again, Rabin's regeneration process (`beatmap_from_analysis.py` workflow) will regen all 3 from existing `*_analysis.json` and re-validate all gates (#125, #134, #135) without re-running `rhythm_pipeline.py` (analysis JSON stable).
- **Team coordination:** Rabin confirmed no independent content correction needed before semantics finalized; standby for post-decision work.
- **Decisions merged:** `.squad/decisions/decisions.md` includes Rabin's content-timing decision + decision metadata.

## 2026-04-27 — Issue #138: 56–64 Beat Silent Gaps in Shipped Beatmaps (Content Regeneration)

**Background:** Ralph identified and fixed silent gaps in mid-song sections exceeding per-difficulty thresholds (Easy ≤40, Medium ≤32, Hard ≤30 beats). Level designer `fill_large_gaps()` pass added to conservatively fill violations.

**Content regeneration scope (post-Ralph → Coordinator → Kujan):**
- All 3 shipped beatmaps regenerated with updated level_designer.py including `fill_large_gaps` pass
- Gap-filling is conservative: only fills violations, distributes fills across lanes to avoid monotony
- Preserves musical intent (chorusses, outros, section rhythm)

**Beatmap validation (Rabin verified):**
- Stomper: easy 32 max gap (✅ limit 40), medium 9 (✅ 32), hard 13 (✅ 30)
- Drama: easy 16 (✅ 40), medium 8 (✅ 32), hard 10 (✅ 30)
- Mental_corruption: easy 33 (✅ 40), medium 31 (✅ 32), hard 23 (✅ 30)
- All content gates preserved: #125 bar coverage, #134 shape-gap, #135 difficulty ramp
- 2395 C++ assertions / 759 test cases pass

**Status:** ✅ Ready for shipping. Rabin's content regeneration complete and validated.

## 2026 — Fresh content diagnostics pass (#169 / #175 / #178)

Re-audited shipped beatmaps (`content/beatmaps/*_beatmap.json`) against `decisions.md` gates (#125/#134/#135/#138) and current tests. All prior gates still pass; surfaced three NEW level-design defects not covered by #44–#162:

- **#169 (AppStore, escalation inversion):** Hard's `lane_push` share is strictly *below* medium's on every shipped song (stomper 15.5%→1.2%, drama 20.3%→6.0%, mental 9.2%→3.9%). #125 bar rotation displaces pushes 2-for-1 with no compensating mechanism, so a taught medium mechanic disappears on hard. Suggested fix: ramp contract `non_shape_share(hard) ≥ non_shape_share(medium) - ε` enforced in `apply_lanepush_ramp` + Catch2 in `test_shipped_beatmap_difficulty_ramp.cpp`.
- **#175 (test-flight, readability cliff):** No spec-defined first-collision floor per difficulty. Hard ships `first.beat=2` on all 3 songs, yielding 0.75–1.00s reaction windows; mental_corruption hard fires obstacles 1.22s after audio start. Suggested floor: easy ≥4.0s, medium ≥2.5s, hard ≥2.0s. test-flight blocker for new players.
- **#178 (AppStore, inconsistent ramp):** Hard bar share varies 1.9–10.5% across songs because `ensure_bar_coverage` (#125) only enforces lower bound and rotation responds to total obstacle count, not duration/density. Suggested band: 4–8% or 1.2–2.4 bars/min, plus upper-bound test.

**Skipped duplicates:**
- 2_drama hard 84% Square / 81% Lane 1 / 0 Circles → covered by #136 acceptance criteria ("measure lane and shape distribution per song/difficulty"). Did not file separately.
- Tail-silence asymmetry (stomper hard ends 0.26s before audio while easy/medium leave 10–12s) → low-impact; held back.
- Per-difficulty same-shape balance (medium/hard lack `balance_easy_shapes` analogue) → folded into #136's "rebalance to meet target ranges" criterion.

**Method:** Python aggregation over shipped JSON; cross-checked against `tools/level_designer.py`, `tests/test_shipped_beatmap_*.cpp`, `design-docs/rhythm-spec.md`. No content edited or regenerated (diagnostic-only charter).

## 2026 — Issue #175: First-collision reaction floor

**Charter task:** Implement TestFlight #175 (hard first obstacle <1 s reaction window) — spec floor + generator pass + Catch2 + content cleanup.

**Spec (`design-docs/rhythm-spec.md`):** Added "First-collision floor (per difficulty)" subsection under §1 Beat Map Format. Codified easy≥4.0s / medium≥2.5s / hard≥2.0s, measured as `offset + first.beat * 60/bpm`. Authoring rule documents postpone-or-drop semantics + cross-references to `enforce_first_collision_floor` and `tests/test_shipped_beatmap_first_collision.cpp`.

**Generator (`tools/level_designer.py`):** Added `MIN_FIRST_COLLISION_SEC` constant and `enforce_first_collision_floor(obstacles, difficulty, analysis)` pass. Walks `analysis["beats"]` to locate the smallest index whose grid time ≥ floor; if leading obstacle's beat is below it, postpones — falling back to drop if postponement would violate per-difficulty `MIN_GAP` against the next obstacle. Wired in as the last step of `design_level` so it sees the final ordered list.

**Test (`tests/test_shipped_beatmap_first_collision.cpp`):** Catch2 `[first_collision][issue175]` loads every `content/beatmaps/*_beatmap.json` × {easy, medium, hard} and asserts `offset + first.beat * 60/bpm` ≥ floor with 1 ms tolerance for `round(offset,3)` quantization. 3 assertions / 2 test cases all pass.

**Content (`content/beatmaps/3_mental_corruption_beatmap.json`):** Only baseline violator. Applied cleanup pass directly: medium first.beat 5→6 (2.421→2.821s, postpone), hard first.beat 3→4 (1.620→2.020s, drop because next entry already at beat 4). Stomper / drama beatmaps left untouched at HEAD — already above floor on every difficulty — to avoid disturbing #125/#134/#135 baselines.

**Validation:**
- `[first_collision]` → all green.
- Pre-existing 8-case / 17-assertion failures in `test_rhythm_system` / `test_collision_system` / `test_beat_map_validation` unchanged (verified via stash + rerun).
- Python validators (`validate_beatmap_bars`, `validate_difficulty_ramp`, `validate_gap_one_readability`, `validate_max_beat_gap`, `validate_medium_balance`, `check_shape_change_gap`) — same failure set as baseline, with `validate_gap_one_readability` slightly improved on mental (medium 14→13, hard 21→19) because postponement skips past two leading gap=1 patterns.

**Comment:** https://github.com/yashasg/friendly-parakeet/issues/175#issuecomment-4323262046 — left open for Saul/Kujan review.

**Lesson — branch reality vs squad-decision history:** `decisions.md` references `clean_shape_change_gap`, `apply_lanepush_ramp`, `ensure_bar_coverage`, `fill_large_gaps` etc. as landed work, but the actual `tools/level_designer.py` on `user/yashasg/ecs_refactor` is the simpler 693-line version without those passes. Many `tests/test_shipped_beatmap_*.cpp` exist only as `.bak` files. When acceptance criteria say "verify #125/#134/#135/#138 gates remain green", the gates aren't green at baseline on this branch — best you can do is "no new failure introduced". Confirm baseline by stashing your edits and re-running validators before claiming "regression-free".

**Lesson — non-destructive content fix beats full regen:** Issue let me choose generator-pass *or* content cleanup. Cleanup-only on the single violating song is safer when the surrounding pipeline doesn't carry the cleaners that newer validators expect; full regen would have produced wider deltas (different first beats on stomper/drama) without buying anything for #175. Keep the generator pass for future regens, but don't re-run the whole pipeline just because you can.

## 2026-05-02 — Root-Cause Analysis: Early Beats as Center Obstacles

**Request:** Explain why early beats become center obstacles despite multiple onset passes.

**Scope:** Diagnostic analysis of `rhythm_pipeline.py` onset detection and `level_designer.py` obstacle placement logic.

**Root Causes (3-layer):**

1. **Primary:** `get_center_obstacle_for_beat()` has no beat-time validation. It reuses obstacles matching shape target without confirming their beat timing aligns with current beat processing. Early transients from aubio multi-onset clustering trigger false reuse.

2. **Secondary:** Multiple aubio onset passes detect same physical beat event as clusters (e.g., 2.27s, 2.28s, 2.29s from instrument overlap). Beat-grid normalization merges to single grid beat, but obstacles placed on intermediate onsets pre-normalization may not match post-normalization beat grid exactly.

3. **Tertiary:** Early song beats occur during intro/fade-in where musical content is sparse. Aubio spectral-change detection flags amplitude ramps and instrument entries as beats. Obstacles on pre-beat transients get reused if shape matches target.

**Evidence:**
- Rabin's prior audit (#137): shipped beatmaps timing-correct to ±1ms under uniform grid semantics.
- `beatmap_from_analysis.py` pipeline relies on `level_designer.py` placement logic.
- No beat-time matching constraint in obstacle-reuse function.

**Recommendations (design-owned by Fenster/Baer):**
1. Tighten `get_center_obstacle_for_beat()` to validate beat-time ±0.05s tolerance.
2. Post-process aubio onsets: merge clusters within 50–100ms before beat-grid extraction.
3. Filter low-energy onsets or skip intro region (pre-4.0s).
4. Regenerate all 3 shipped beatmaps post-fix; re-validate #125/#134/#135/#138 gates.

**Status:** ✅ Analysis complete, no code changes per charter. Orchestration log: `.squad/orchestration-log/2026-05-02T07-26-41Z-rabin.md`. Session log: `.squad/log/2026-05-02T07-26-41Z-scribe-session.md`.

## 2026-05-09 — Loop 1 content acceptance gates (beatmap quality)

**Charter:** Translate the 2_drama defects from Sr Game Designer's beat
review (gap monotony, same-shape runs, shape distribution, off-beat
material, density) into numeric content gates for Loop 1, separating
hard gates (block ship) from diagnostic warnings (escalate). No
runtime/schema changes; `tools/level_designer.py` owned by Fenster
this loop, so the gates are written against the existing schema only.

**Method:** Re-measured `content/beatmaps/2_drama_beatmap.json` at
HEAD of `beatmap/quality-improvements` and computed gap distribution,
same-shape runs, shape shares, longest runs, and a phase-class
histogram of analysis events. Cross-referenced against the Sr Game
Designer review's AC-1..AC-8 and Rabin's subdivision charting note.

**Key new numbers (not in Sr Game Designer review):**
- Hard's longest same-shape run is **circle×26** (the review tracked
  4+ within section windows; whole-chart contiguous run is 26).
- Of 216 detected onsets, **72.2% are sub-beat** (51.4% eighths,
  20.8% triplets) — the off-beat material exists, the generator
  discards it via the 80 ms snap window.
- Medium: gap=2 41.3% (FAIL HG-1 by 1.3 pp), longest run tri×11.
- Hard: gap=2 51.3% (FAIL HG-1 by 16.3 pp), triangle 1.5% (FAIL
  HG-3 floor of 25%), circle 51% (FAIL HG-3 ceiling of 40%).

**Gate structure delivered (artifact §2/§3):**
- Hard gates HG-1..HG-6: gap-monotony cap, same-shape-run cap,
  shape distribution band, absolute-time IOI floor, gap=1 burst cap,
  internal-consistency hygiene (count == len(beats), valid indices).
- Diagnostic warnings DW-A..DW-F: gap-distribution entropy, lane
  monotony, 30 s shape drought, gap=2 vs neighbour skew, subdivision-
  shape mismatch (read-only diagnostic that produces the Loop 2
  schema-change argument), and run-cap saturation.

**Loop 1 success rubric:** filled per-difficulty target columns for
medium and hard against today's measured baseline (artifact §4).
"All-HG-pass" required to ship; ≥2 DW trips on the same difficulty
escalates to Sr Game Designer review.

**Out-of-scope (deferred):**
- Bar obstacles (AC-1) — branch lacks LowBar/HighBar; Loop 2+.
- Sub-beat placement — needs schema change; Loop 2+, with DW-E
  producing the corpus to argue for it.
- Engine offset/drift (AC-6) — engine-side, not content.

**Artifact:**
`/Users/yashasgujjar/.copilot/session-state/c0ddd445-5e34-4aa9-bc53-563866a0574f/files/beatmap-quality-loop1-2026-05-09/rabin-content-gates.md`

**Open questions logged for Sr Game Designer / Fenster** (artifact §7):
HG-3 25% triangle floor on hard, HG-5 20% gap=1 ceiling defensiveness,
DW-B 20% per-lane floor borrowed from Issue #169 stomper audit.

**Lesson — measure the chart, don't trust review summaries verbatim.**
The Sr Game Designer review reported "9 consecutive squares / triangle
×11" as worst-case but used a section-window pass; a whole-chart pass
on hard finds **circle×26**. Set HG-2 against the worst observed
contiguous run (≤3), not the review's 4+ threshold, otherwise the
gate ships pre-broken.

**Lesson — separate "fix the defect" from "don't introduce a new
defect."** HG-1 alone could be satisfied by dumping every excess
gap=2 into gap=1, recreating mashing. HG-5's gap=1 cap is the
regression guard. Pair every redistribution gate with a sink-side
ceiling.

---

## 2026-05-09 — Onset spike playability review (`beatmap/onset-obstacle-spike` @ e80211a)

**Artifact:** `tools/diagnostics/onset_spike/rabin-playability-review.md`
**Decision:** `.squad/decisions/inbox/rabin-onset-spike-playability.md`

**Headline findings**

- **Shape ↔ lane is hard-coded 1:1** in every shipped beatmap
  (`lane == shape_index`). Every "same-shape run" is also a "same-lane
  run." This is the dominant playability bug post-spike.
- Same-shape runs at hard: stomper **42**, mental **43**, drama **21**.
  Stomper easy also has a run of **40** — FTUE-breaking.
- `subdivision_label` is **100% `downbeat`** on every obstacle of every
  chart, even though the detector resolves eighths/triplets in the
  source events. Strict label gate failure is real, not a gate bug.
- Stomper has a difficulty curve **inversion** — easy median IOI 755 ms
  vs hard 1074 ms.
- Mental has **flat 806 ms median IOI** across easy/medium/hard. Hard
  is just "longer easy with more walls."
- Mental easy has only **2 triangle obstacles in 110** — effectively a
  2-lane chart that doesn't teach lane 2 before medium hits 48
  triangles.
- Onset source share is tiny on stomper (4–6 %) and drama (6–8 %); the
  spike is mostly a Mental Corruption feature today (14/19/28 %).

**Lessons**

- *Always recompute longest same-shape runs straight off the JSON.* The
  diagnostics summary's `same_shape_run_metrics` is correct, but seeing
  the actual run sequence (`[38, 5, 42, 15, 13, 14, 27]` for stomper
  hard) makes it obvious that one cap value isn't enough — the
  distribution matters.
- *Treat lane and shape as orthogonal even if today's authoring couples
  them.* Same-shape gates that don't also cap same-lane runs will be
  bypassed the moment lane assignment is decoupled, so propose both
  ceilings together (D2).
- *Strict label-coverage gates need an authoring fix, not a gate
  relaxation.* The subdivision data exists upstream; it's the
  candidate→event step that drops the label.
- *Use `current_quarter_snap.count` vs `experimental_onset_timing.event_counts.obstacles`
  ratio as a quick sanity check on whether a "spike" is actually
  exercising onset timing on a given song.* On stomper that's
  25 / (178+154+196) ≈ 5 %, matching the per-difficulty histogram.
