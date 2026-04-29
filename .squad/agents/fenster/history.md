# Fenster — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Tools Engineer
- **Joined:** 2026-04-26T02:07:46.545Z

## Learnings

<!-- Append learnings below -->

## 2026-04-26 — Beatmap Pipeline Diagnostic

- `SECTION_ROLE` and `SECTION_SHAPE_PALETTE` in `level_designer.py` only define: `intro`, `verse`, `pre-chorus`, `drop`, `bridge`. Missing: `chorus`, `outro`. The `_label_sections()` function in `rhythm_pipeline.py` **can produce** `chorus` and `outro` labels. All 3 shipped songs have chorus and/or outro sections. The fallback `SECTION_ROLE.get(sec, SECTION_ROLE["verse"])` silently applies verse-density to these sections, making choruses under-populated relative to drops and outros over-populated (should wind down).

- `high_bar` and `low_bar` are spec-valid obstacle types (`rhythm-spec.md`) and fully wired in the C++ loader + scheduler. However, `DIFFICULTY_KINDS` in `level_designer.py` never includes them, so `assign_obstacle` computes `natural_kind = "high_bar"|"low_bar"` then immediately falls back to `shape_gate`. The variety injection code that references `low_bar`/`high_bar` in `sec_kinds` is also unreachable. These types have zero real-world content coverage.

- `run_aubio()` in `rhythm_pipeline.py` uses `subprocess.run()` with no `timeout=` argument. A hung aubio process (e.g., corrupt audio) blocks the pipeline indefinitely.

- Analysis `duration` is computed as `max(beats + all_onset_timestamps) + 1.0s`. For songs with long fade-outs this underestimates real audio duration. The C++ `song_playback_system` ends the game when `song_time >= duration_sec`, so underestimation = premature game-over.

- Install docs say `pip install aubio numpy` but the pipeline uses the `aubio` **CLI** (via subprocess), not the Python module. On macOS the CLI comes from `brew install aubio`, not pip. The Python `aubio` package installs bindings, not the CLI.

- `validate_beat_map()` (C++) is not called by `load_beat_map()`. Callers must invoke it manually. Easy to miss.

- `validate_beat_map()` doesn't validate lanes for `LanePushLeft`/`LanePushRight` (only does so for `ShapeGate`/`SplitPath`). An out-of-range lane on a push obstacle would cause a bad `LANE_X[]` array index in `beat_scheduler_system`.

- `clean_level()` docstring omits `clean_minimum_gap` from its ordered step list (it runs first, before the 5 listed steps).

- `beatmap-editor/js/io.js` `validate()` reports "empty beats" as `severity: 'warning'`; the C++ `validate_beat_map` treats it as a hard error. Severity inconsistency.

- aubio `tempo` output on macOS is `"146.24 bpm"` (not just a bare float). `get_tempo()` correctly handles this with `.split()[0]`.

- All 3 shipped audio assets are FLAC; aubio CLI supports FLAC directly. The `rhythm_pipeline.py` docstring only shows `.wav` in its usage example but `.flac` works fine.

- `content/beatmaps/1_stomper_analysis.json` has BPM=159.01, but re-running `aubio tempo` gives 146.24 — a 13 BPM drift. No freshness mechanism (hash/timestamp) exists to detect stale analysis files.

## 2026-04-27 — Issue #137: Offset Semantics Fix

- **Root cause of #137:** `offset = beats[0]` anchors the beat grid to the first aubio-detected beat, which may be an unreliable early transient (false positive before the true first downbeat). If `beats[0]` is off by even one beat period (e.g., 377ms at 159 BPM), every single collision shifts by that amount — highly noticeable.

- **Offset semantics confirmed:** `offset` = audio time (seconds from audio start) of beat_index=0 in the uniform beat grid. Runtime formula: `arrival_time = offset + beat_index * (60/bpm)`. This is a **uniform grid model** — real aubio timestamps are not stored per-beat at runtime; only the anchor + BPM matter.

- **Fix:** In `level_designer.py::build_beatmap()`, compute `offset = beats[anchor_idx] - anchor_idx * beat_period` where `anchor_idx = min(all authored beat indices across all difficulties)`. This guarantees the first authored collision is on-beat regardless of `beats[0]` reliability.

- **For current 3 songs:** The change is < 1ms (stomper: 2.270 → 2.269, drama: no change, mental: no change) because the existing songs have reliable first beat detections. Zero gameplay regression.

- **Semantics are now explicit in 3 places:** `beat_map.h` (struct field doc comment), `level_designer.py::build_beatmap()` (formula comment), `beat_scheduler_system.cpp` (arrival_time comment).

- **New validation tool:** `tools/validate_beatmap_offset.py` — checks that every shipped beatmap's offset is correctly anchored to its first authored beat within 2ms tolerance. Run from repo root; exits 0 on success.

- **Key insight on uniformity:** For typical electronic music (fixed BPM, no tempo changes), the uniform grid model is accurate to < 1ms per beat. The anchor choice only matters when `beats[0]` is unreliable — the fix is cheap insurance.

## 2026-04-27 — Issue #137 Complete: Team Outcome

- **Status:** ✅ APPROVED. Issue #137 (offset semantics and off-beat collision fix) has been completed, tested, reviewed, and approved.
- **Team deliverables:**
  - Fenster: Offset semantics defined + implemented in pipeline and C++ docs
  - Rabin: Content timing audit completed (±1ms accuracy verified); content regenerated (1ms delta on stomper)
  - Baer: Added `test_beat_scheduler_offset.cpp` (9 tests) + `validate_offset_semantics.py` (7 suites) for regression protection
  - Kujan: Full review gate passed; no blocking issues
- **All gates pass:** 2392 assertions, 757 test cases; #125, #134, #135 validators all green
- **Orchestration log:** `.squad/orchestration-log/2026-04-27T00:00:06Z-fenster.md`
- **Session log:** `.squad/log/2026-04-27T00:00:07Z-issue-137-offset-semantics.md`
- **Decisions merged:** `.squad/decisions/decisions.md` (consolidated 4 inbox decisions; inbox cleared)

## 2026-04-28 — Fresh Diagnostic Session (Issues #222–#230)

### Reviewed scope
Input artifacts: `tools/`, `content/beatmaps/`, `content/audio/`, `app/beat_map_loader.cpp`, `app/systems/audio_system.cpp`, `app/systems/beat_scheduler_system.cpp`, `tests/`.

Checked all existing issues #44–#220 for overlap before filing.

### Duplicates skipped
- #86 (clean_level omits clean_minimum_gap) — still open; #86 is the root, #228 is the follow-on for additional missing steps
- #126 (run_aubio timeout) — still open; only overlaps conceptually with #222 (different function)
- #131 (empty-beats warning vs. error) — still open; only overlaps class-of-issue with #227
- #92 (same-beat validator) — was about C++ validator, now fixed in C++; #226 covers the remaining JS side

### New issues filed

| Issue | Title | Milestone |
|---|---|---|
| #222 | get_audio_duration subprocess has no timeout | test-flight |
| #224 | parse_beat_map silently defaults missing 'shape' to Circle on shape-bearing obstacles | AppStore |
| #226 | Beatmap editor validate() rejects valid same-beat simultaneous obstacles (too-strict monotonicity) | AppStore |
| #227 | Beatmap editor validate() reports shape-change-gap as 'warning'; C++ treats as hard error | AppStore |
| #228 | clean_level docstring omits clean_shape_change_gap and second clean_two_lane_jumps passes | AppStore |
| #229 | fill_large_gaps does not fill trailing silence after last obstacle | AppStore |
| #230 | _build_structure_from_quiet fallback never emits 'drop' or 'pre-chorus' labels | AppStore |

### Key learnings

- `get_audio_duration()` in `rhythm_pipeline.py` uses `subprocess.run()` with no `timeout=` — same pattern as #126 but different function. Added as #222.

- `parse_beat_map()` never validates that shape-bearing obstacle kinds carry an explicit `"shape"` field. Shipped content is safe (generated by level_designer.py which always sets shape), but hand-authored beatmaps can silently default to Circle.

- JS editor `io.js` `validate()` uses `entry.beat <= prevBeat` (strict monotonic) while C++ loader uses `entry.beat_index < prev_beat` (non-decreasing, allows same-beat multi-action patterns). The JS editor false-positives on valid same-beat patterns the C++ runtime accepts.

- JS editor shape-change-gap severity (`'warning'`) disagrees with `validate_beat_map` return value (`false`/hard error), though the runtime has a non-fatal path for this specific error. Three-way inconsistency between editor, validator, and runtime loader.

- `clean_level()` docstring was updated after #86 to include `clean_minimum_gap` (step 1), but two steps added in the #134 / #86-fix era (`clean_shape_change_gap` and second `clean_two_lane_jumps`) remain undocumented.

- `fill_large_gaps()` only fills gaps between consecutive obstacles; trailing silence after the last obstacle is never examined or filled.

- `_build_structure_from_quiet()` fallback (triggered when `len(mfcc_times) < 100`) never assigns `"drop"` or `"pre-chorus"` labels, even though those sections have higher density and richer obstacle palettes. Affects pipeline robustness for short/corrupt audio.

- All shipped beatmaps verified: beat indices within max_beat, no same-beat simultaneous entries, no shape-change-gap violations, all shape-bearing obstacles have explicit shape fields.

- `sfx_bank_init` and `sfx_bank_unload` call sites confirmed: init is called in `audio_system.cpp` (lazy) and `game_loop.cpp` (eager); unload is called in `game_loop.cpp` on shutdown. No leak.

- `AudioQueue::MAX_QUEUED = 16` is a silent drop limit. Dense frame SFX beyond 16 are discarded but do not crash. Not filed (acceptable design trade-off at current density).

## 2026-04-28 — Issue #222: get_audio_duration timeout fix

- **Root cause:** `get_audio_duration()` in `rhythm_pipeline.py` called `subprocess.run()` with no `timeout=` argument. A hung `ffprobe` (corrupt file, network path, broken binary) would block the pipeline indefinitely.

- **Fix:** Added `FFPROBE_TIMEOUT = 30` constant (plus `AUBIO_TIMEOUT = 120` reserved for #126). Updated `get_audio_duration()` to pass `timeout=FFPROBE_TIMEOUT`, catch `subprocess.TimeoutExpired`, emit a `stderr` warning, and return `None`. No change to the happy path or existing `FileNotFoundError`/non-zero-exit handling.

- **Pattern established:** `rhythm_pipeline.py` now has a single constants section for subprocess timeouts (`FFPROBE_TIMEOUT`, `AUBIO_TIMEOUT`). When #126 is addressed for `run_aubio()`, use `AUBIO_TIMEOUT` from that same section.

- **Tests added:** `tools/test_get_audio_duration.py` — 9 `unittest`-based tests (no external deps beyond stdlib). Covers: happy path, timeout constant forwarded, FileNotFoundError, TimeoutExpired (None + stderr + no-raise), non-zero exit, zero/negative/unparseable output. Run with `python3 tools/test_get_audio_duration.py`.

- **GitHub comment:** https://github.com/yashasg/friendly-parakeet/issues/222#issuecomment-4323246963

---

**Wave Summary (2026-04-26T23:40:25Z):**
- Diagnostics filed 7 new issues (#222–#230): aubio timeout (#222, test-flight blocker), parse_beat_map defaults, JS editor strictness, clean_level gaps, structure fallback, etc.
- Fenster findings merged to `.squad/decisions.md` (inbox deleted).
- No duplicates found with #44–#162 baseline.

### 2026-05-17 — EnTT ECS Audit: Constant Deduplication (F2/F3)

**Context:** Kujan's audit review identified `COLLISION_MARGIN` (tripled in 3 files) and `APPROACH_DIST` (duplicated between header and constant file) as medium-risk drift vectors. When promoted/consolidated to `app/constants.h`, both are now authoritative single-source-of-truth.

**Status:** Assigned to McManus (primary), Fenster (tooling/constants). Part of EnTT ECS remediation backlog (low-effort, low-risk fixes). Orchestration log: `.squad/orchestration-log/2026-04-27T19-14-36Z-entt-ecs-audit.md`.

## 2026-04-28 — Non-Component Header Cleanup

- Deleted `app/components/audio.h` (duplicate of `app/systems/audio_types.h`) — updated all includes across app/, tests/, benchmarks/.
- Deleted `app/components/music.h` (duplicate of `app/systems/music_context.h`) — updated all includes.
- Deleted `app/components/settings.h` (duplicate of `app/util/settings.h`) — updated all includes including `app/util/settings_persistence.h`.
- Moved `app/components/shape_vertices.h` → `app/util/shape_vertices.h` (vertex data, not an ECS component) — updated game_render_system.cpp, test_perspective.cpp, bench_perspective.cpp.
- Deleted `app/components/obstacle_counter.h` (duplicate of `app/systems/obstacle_counter_system.h`) — `obstacle_counter_system.cpp` now explicitly includes `obstacle_counter_system.h`.
- Deleted `app/components/obstacle_data.h` — folded `RequiredShape`, `BlockedLanes`, `RequiredLane`, `RequiredVAction` into `app/components/obstacle.h` (added `#include "player.h"` there for Shape/VMode types).
- Benchmarks dir also had stale includes — fixed bench_systems.cpp and bench_perspective.cpp.
- Build and all 2978 test assertions pass after cleanup.

---

### 2026-04-28 — Team Session Closure: ECS Cleanup Approval

**Status:** APPROVED ✅ — Deliverable logged; ready for merge.

Scribe documentation:
- Orchestration log written: .squad/orchestration-log/2026-04-28T08-12-03Z-fenster.md
- Team decision inbox merged into .squad/decisions.md
- Session log: .squad/log/2026-04-28T08-12-03Z-ecs-cleanup-approval.md

Next: Await merge approval.

---

## Session: 2026-04-28T22:35:09Z — Audio Abstraction Cleanup Consolidated

**Context:** Scribe merged inbox decisions into decisions.md and updated cross-agent history files.

**Your work:** Audio abstraction boundary cleanup (app/audio/ deleted; all audio types consolidated to canonical ECS and utility locations). Validated: shapeshifter and tests build zero-warning; Unity build clean (no ODR collisions). 297 test assertions pass.

**Status:** Audio cleanup complete and merged into team decisions.

**Related:** `fenster-audio-boundary-cleanup.md` merged into `.squad/decisions.md`

---

## Session: 2026-04-28T17:xx:xxZ — rguilayout Integration Path Setup

**Context:** Set up concrete, compile-safe integration path for rguilayout-generated UI layouts to replace JSON-driven UI system.

**Deliverables:**
1. ✅ Custom embeddable template: `tools/rguilayout/templates/embeddable_layout.h` (header-only, no main, no RAYGUI_IMPLEMENTATION)
2. ✅ Manual embeddable proof artifact: `app/ui/generated/title_layout.h` (compile-safe, 122 lines)
3. ✅ Thin adapter: `app/ui/adapters/title_adapter.{cpp,h}` (bridges generated layout to game systems)
4. ✅ Standalone files archived: `app/ui/generated/standalone/*.{c,h}` (NOT built, reference only)
5. ✅ Integration documentation: `tools/rguilayout/INTEGRATION.md` (6.5KB, complete path and limitations)
6. ✅ Generation helper script: `tools/rguilayout/generate_embeddable.sh`

**Key Findings:**
- **rguilayout template substitution is incomplete**: The `--template` flag does not properly substitute template variables like `$(GUILAYOUT_INITIALIZATION_C)` or `$(GUILAYOUT_DRAWING_C)`. The portable `.h` template uses different variables (`$(GUILAYOUT_STRUCT_TYPE)`, `$(GUILAYOUT_FUNCTION_DRAWING_H)`) that produce nested/malformed code.
- **Manual generation required**: Embeddable headers must be manually crafted from standalone output by extracting initialization and drawing code blocks.
- **Standalone files are dangerous**: Default exports include `main()` and `RAYGUI_IMPLEMENTATION` - must never be added to build. Moved to `generated/standalone/` archive.

**Compile Safety Verified:**
- ✅ Build passes with zero warnings after moving standalone files
- ✅ No CMake changes (adapters not wired yet - future work)
- ✅ JSON UI runtime still active (no regression)
- ✅ Generated layout header is header-only STB-style (safe to include multiple times)

**Integration Path (deferred to future build-integration task):**
1. Add raygui.h to include path (vendor or vcpkg)
2. Define RAYGUI_IMPLEMENTATION in single .cpp file
3. Wire adapters into CMakeLists.txt
4. Call adapters from ui_render_system
5. Add compile-time flag to switch JSON vs rguilayout (default OFF)
6. Generate embeddable headers for remaining 7 screens

**Safety Rules Established:**
- ❌ Never include standalone generated files (they have main())
- ❌ Never copy layout rectangles into ECS components
- ❌ Never create layout cache structs (HudLayout, LevelSelectLayout, etc.)
- ✅ Only compile embeddable headers via adapters
- ✅ Define RAYGUI_IMPLEMENTATION once in entire binary
- ✅ Keep JSON UI active until rguilayout validated

**Files Changed:**
- Created: `tools/rguilayout/templates/embeddable_layout.h`
- Created: `app/ui/generated/title_layout.h`
- Created: `app/ui/adapters/title_adapter.{cpp,h}`
- Created: `tools/rguilayout/INTEGRATION.md`
- Created: `tools/rguilayout/generate_embeddable.sh`
- Created: `app/ui/generated/standalone/README.md`
- Moved: 16 standalone `.{c,h}` files → `app/ui/generated/standalone/`

**Team Decision:** See `.squad/decisions/inbox/fenster-rguilayout-template-integration.md`

---

## 2025-04-28 — rguilayout Title Screen Runtime Integration

**Context:** User requested rguilayout title screen to render in-game. Existing work:
- `.rgl` sources under `content/ui/screens/` (authored layouts)
- Generated header `app/ui/generated/title_layout.h` (embeddable, header-only)
- Thin adapter `app/ui/adapters/title_adapter.{cpp,h}` (not wired into build)
- raygui.h was NOT vendored yet

**Implementation:**
1. **Vendored raygui.h**: Downloaded from https://github.com/raysan5/raygui into `app/ui/vendor/raygui.h` (6056 lines, v5.0)
2. **Created raygui implementation TU**: `app/ui/raygui_impl.cpp` with `#define RAYGUI_IMPLEMENTATION` wrapped in compiler-specific warning suppression pragmas (`-Wmissing-field-initializers`, `-Wunused-parameter`) to comply with zero-warnings policy
3. **Fixed generated header extern "C" issues**: Removed redundant `extern "C"` blocks in implementation section and converted functions to `static inline` within the header's extern "C" block to avoid C/C++ linkage mismatch
4. **Fixed nested comment issue**: Changed `/* ... */` comments inside the header's multi-line `/* ... */` block comment to `//` style
5. **Fixed aggregate initialization warning**: Changed `TitleLayoutState state = {0};` to `TitleLayoutState state = {};` (C++20 empty braces)
6. **Added UI adapter to CMake**: Added `file(GLOB UI_ADAPTER_SOURCES ...)` and included in `shapeshifter_lib`
7. **Marked raygui_impl.cpp for unity build exclusion**: Added `SKIP_UNITY_BUILD_INCLUSION` property to avoid ODR violations (raygui defines hundreds of static functions)
8. **Wired title adapter into ui_render_system**: Added `#include "../ui/adapters/title_adapter.h"` and `case ActiveScreen::Title:` dispatch that calls `title_adapter_render(reg)`
9. **Fixed const correctness**: Changed `title_adapter_render` signature to `const entt::registry&` to match `ui_render_system`'s const ref

**Build & Test Results:**
- ✅ Zero-warning build on macOS arm64 with clang++ `-Wall -Wextra -Werror`
- ✅ All 2631 assertions pass in 901 test cases
- ✅ Game launches successfully with raygui title screen rendering at startup

**Key Technical Learnings:**

1. **Nested extern "C" blocks don't work in C++**: When a header declares types/functions in `extern "C" {}` and then an implementation section re-opens `extern "C" {}`, the implementation functions have C linkage but the types from the closed block don't carry over cleanly in C++ mode. Solution: make functions `static inline` within the header's single `extern "C"` block.

2. **raygui needs warning suppression for zero-warning builds**: raygui v5.0 has `-Wmissing-field-initializers` (uses `Rectangle r = {0}` idiom) and `-Wunused-parameter` warnings. Wrap `RAYGUI_IMPLEMENTATION` in pragma push/pop blocks.

3. **Nested block comments are illegal**: `/* ... /* inner */ ... */` is not valid C/C++. Generated headers with example code in block comments must use `//` for inner code comments.

4. **Unity builds and header-only libraries**: raygui defines hundreds of static functions when `RAYGUI_IMPLEMENTATION` is set. The TU that defines it must be excluded from unity builds via `SKIP_UNITY_BUILD_INCLUSION`.

5. **C++20 aggregate init**: `{0}` triggers `-Wmissing-field-initializers` for structs with multiple fields; `{}` (empty braces) is the correct C++20 zero-init syntax.

**Runtime Status:**
- rguilayout title screen now renders in-game when `ActiveScreen::Title` is active
- Button actions NOT wired yet (settings navigation, exit dispatch) — rendering-only for now
- JSON UI path still active for other screens (LevelSelect, Gameplay HUD, etc.)

**Files Changed:**
- `app/ui/vendor/raygui.h` (new, vendored from upstream)
- `app/ui/raygui_impl.cpp` (new, single RAYGUI_IMPLEMENTATION site)
- `app/ui/adapters/title_adapter.{cpp,h}` (updated: const registry, removed TITLE_LAYOUT_IMPLEMENTATION)
- `app/ui/generated/title_layout.h` (fixed: extern C, nested comments, aggregate init, static inline)
- `app/systems/ui_render_system.cpp` (added title_adapter.h include, ActiveScreen::Title case)
- `CMakeLists.txt` (added UI_ADAPTER_SOURCES glob, raygui_impl.cpp unity exclusion)

**Next Steps (if needed):**
- Wire button actions to game dispatchers (settings nav, exit handler)
- Migrate remaining 7 screens to rguilayout adapters (level_select, gameplay HUD, etc.)
- Add feature flag to toggle JSON vs rguilayout at compile-time (default rguilayout)

## 2026-04-29T02:45:27Z — Session Wrap: raygui Title Runtime Integration APPROVED

**Status:** Decision #189 APPROVED and merged into decisions.md. Work complete.

**Team Communications:**
- **Hockney:** Final validation passed. Title screen renders correctly, zero warnings, tests pass.
- **Keyser:** Integration architecture confirmed working end-to-end.
- **Scribe:** Orchestration logs created, inbox merged into decisions.md.
- **All agents:** Pattern established for future screen migrations (1/8 title screen done).

**Deferred (Intentional):**
- Button action wiring (visuals complete; event system refactor needed)
- Remaining 7 screens (7/8 screens still JSON-driven, incremental adoption preserved)
