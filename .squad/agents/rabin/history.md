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
