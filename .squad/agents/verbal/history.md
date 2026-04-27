# Verbal — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** QA Engineer
- **Joined:** 2026-04-26T02:07:46.547Z

## Learnings

### 2026-04-27 — Revision #135: Easy shape_gate_only contract guard

- **Kujan rejected Baer's #135 artifact** because easy tests checked shape *variety* but not obstacle *kind*. This allowed Rabin's lane_push contamination in easy beatmaps to silently pass.
- **Root cause pattern:** Checking distribution within a category (shape variety among shape_gates) does not imply purity of that category. Whenever a difficulty contract restricts *which kinds* are allowed, a separate kind-exclusion test is mandatory alongside distribution checks.
- **Fix applied:** Added `[shape_gate_only]` test case to `tests/test_shipped_beatmap_difficulty_ramp.cpp` and `check_easy_shape_gate_only()` function to `tools/validate_difficulty_ramp.py`. Both use `FAIL_CHECK`/list-accumulation so every violation per file is reported in a single run.
- **Confirmed guard works:** C++ test exits 42 (10 failures across 3 beatmaps) on current dirty content; 4 existing tests unaffected. Python exits 1 with 10 explicit violation lines.
- **Ready for McManus:** Once McManus removes lane_push from easy beatmaps, both guards will go green with no further test changes needed.

## 2026-04-27 · #135 APPROVED: Verbal's test additions

- **Task completion:** Added kind-exclusion assertions to test suite.
- **Convention established:** Every difficulty contract test must include (1) kind-exclusion assertions and (2) distribution/variety assertions. This two-layer approach catches both *presence* violations and *distribution* violations.
- **C++ additions:** New `[shape_gate_only]` test case in `tests/test_shipped_beatmap_difficulty_ramp.cpp` iterates every beat in all shipped easy beatmaps, calls `FAIL_CHECK` on any banned obstacle kind (LanePushLeft, LanePushRight, LowBar, HighBar).
- **Python additions:** New `check_easy_shape_gate_only()` function in `tools/validate_difficulty_ramp.py` with `EASY_BANNED_KINDS` set, exits non-zero on violation.
- **Result:** McManus's beatmap fixes pass guards green. All 2366 C++ assertions pass (730 test cases). Difficulty ramp tests all pass.
- **Approved by Kujan.** Both blocking issues from prior rejection resolved.

## 2026-05-XX — Fresh QA diagnostics (issues #214, #217, #219, #221)

**Task:** Fresh coverage diagnostics. Audit test suite vs. implementation for unguarded regressions. Search #44–#162 to avoid duplicates.

**Approach:** Read all test files, key system implementations (collision_system, scoring_system, energy_system, beat_scheduler_system, player_movement_system, cleanup_system), design docs, and decisions.md. Cross-referenced against known issues.

**Findings — 4 new issues filed:**

| Issue | Title | Milestone |
|-------|-------|-----------|
| #214 | SongResults good/ok/bad timing counts never verified via collision_system | AppStore |
| #217 | Medium beatmap LanePush start_progress contract has no C++ regression guard | test-flight |
| #219 | Hexagon shape guard is untested for ComboGate and SplitPath collision paths | AppStore |
| #221 | scoring_system timing_mult integration untested for Good, Ok, and Bad tiers | AppStore |

**Duplicates skipped:**
- #157: LanePushLeft/Right collision tests → already filed, not duplicated
- #162: Chain bonus scoring tests → already filed
- #161: Burnout zone boundary tests → already filed
- #160: EnergyState reset on enter_playing → already filed
- #158: Input swipe threshold tests → already filed

**Learnings:**

- **Coverage pattern:** When a switch/case in a system handles multiple enum values with the same logic, individual tests per enum value may be skipped if one path is tested. Always verify each enum branch has an integration-level test, not just the most prominent one.
- **Counter asymmetry:** When a struct has N parallel counters (perfect/good/ok/bad/miss), verify all N are covered via integration paths — not just the most prominent (perfect) or via manual-injection (UI resolver).
- **Generator-enforced contracts:** Content contracts enforced only by the Python generator (not validated at load time or asserted in C++) are invisible to CI. Any content contract that would matter to players should have a C++ shipped-beatmap regression test. Reference: start_progress gap from #135.
- **Multi-obstacle type guards:** When the same logic (e.g., Hexagon rejection) is copy-pasted across multiple switch branches, each branch needs its own test — the guard is logically identical but independently removable.

## 2026-XX-XX — #167 AC#5 Revision (tautological test fix)

- **Kujan rejected McManus's AC#5** because the test manually emplaced `BankedBurnout`, then wrapped the "overwrite" attempt in `if (!reg.any_of<BankedBurnout>(gate))` — a branch that can never run. The final assertion checked the manually-injected value, exercising nothing in `player_input_system`.
- **Root cause pattern:** When testing a "first-commit-lock" guard, never manually emplace the component being locked. The only valid approach is to drive the system with real events twice, then assert the first value survived. If any step in the "second commit" is gated on the component's absence, the test is a no-op.
- **Fix:** Replaced with a two-pass event-driven test: `push_press` at dist=600 → `player_input_system` (banks Safe), move gate to dist=200, `push_go(Direction::Right)` → `player_input_system` (lock holds). `ComboGate` is the canonical fixture because `player_input_system`'s GoEvent path also calls `bank_burnout` on it — both banking paths share the same lock.
- **Sentinel pattern:** A meaningful first-commit-lock test must have a clear failure mode: removing the `reg.any_of<BankedBurnout>(target)` guard must cause the second-pass assertion to fail.
- **Validation:** 26 assertions / 11 test cases in `[burnout_bank]` all pass; 62 assertions / 34 test cases in `[player]` all pass. Comment posted at https://github.com/yashasg/friendly-parakeet/issues/167#issuecomment-4323307927.

## 2026-XX-XX — PR #43 CI Fix: Bar miss tests missing scoring_system call

**Task:** Fix CI failures in `[collision]` tests after #280 moved miss energy drain / flash_timer ownership from `collision_system` to `scoring_system`.

**Failing tests:**
- `collision: low bar drains energy when sliding` (test_collision_system.cpp:286-287)
- `collision: high bar drains energy when jumping` (test_collision_system.cpp:300-301)

**Root cause:** Both bar-miss tests called only `collision_system` then asserted `energy.energy < 1.0f` and `energy.flash_timer > 0.0f`. After #280, those side-effects are produced by `scoring_system` (which processes `MissTag`). Every other miss-drain test in the file already followed the two-step pattern: `collision_system` → `scoring_system`.

**Fix:** Added `scoring_system(reg, 0.016f)` after `collision_system` in both failing test cases. No assertions weakened; no gameplay ownership changed.

**Validation:** `./build/shapeshifter_tests "[collision]"` → 105 assertions / 49 test cases, all pass.

**Commit:** dca7664 on branch `user/yashasg/ecs_refactor`.

**Pattern to remember:** Whenever a miss-drain test asserts energy or flash_timer, it must call `collision_system` then `scoring_system` — never just `collision_system` alone. MissTag is stamped by collision; the energy/flash side-effects are owned by scoring.
