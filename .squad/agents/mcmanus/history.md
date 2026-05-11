# McManus — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Gameplay Engineer
- **Joined:** 2026-04-26T02:07:46.544Z

## 2026-04-29 — Gameplay shape buttons migration (first pass → rejected)

**Status:** FIRST IMPLEMENTATION PASS
**Reviewer:** Kujan
**Verdict:** REJECTED (visual regression — stock rectangular overlay)

**Work completed:**
- Removed `spawn_playing_shape_buttons()` body and caller in `setup_play_session.cpp`
- Removed `ShapeButtonTag` branches from `hit_test_handle_input` loop
- Implemented shape press tracking in `GameplayHudLayoutState`
- Implemented `gameplay_hud_apply_button_presses(...)` to emit semantic `ButtonPressEvent` shape presses
- All tests passing; build warning-clean

**Blocker:** Stock rectangular raygui `GuiButton` visuals (from generated `GameplayHudLayout_Render()` calls) overlay custom circular shape visuals and approach rings, violating Redfoot's circular-silhouette UX requirement.

**Reassignment:** Fenster (Tools Engineer, non-locked) to fix visual overlay while keeping raygui ownership path.

**See:** `.squad/orchestration-log/2026-04-29T22-03-09Z-mcmanus.md`

---

## 2026-04-30T02:04:27Z — Dead Code Prune — Stale Doc Cleanup (Rejected, rework by Fenster)

**Session:** Multi-agent dead code cleanup.

**Your role:** Doc cleanup in `design-docs/raygui-rguilayout-ui-spec.md`, `design-docs/feature-specs.md`, `design-docs/architecture.md` for removed `ui_navigation_system`, `UIState`/`ActiveScreen`, legacy hit-test references.

**Outcome:** ❌ REJECTED by Kujan. Wording still implied raw input routing emits `ButtonPressEvent` (it doesn't). Fenster revised independently under lockout; clarifications approved. Input routing semantics now correctly documented: raw input → `InputEvent` → `GoEvent`; `ButtonPressEvent` from raygui/controller emitters only.

## Learnings

### 2026-04-30 — Song complete UI missing + music looping

- **Root cause:** Loaded music streams were never forced to one-shot mode. raylib music defaults can loop, causing playback to wrap and continue instead of naturally terminating, so `song_time` never reached terminal song completion in real runs.
- **Production fix:** Set `music->stream.looping = false` immediately after successful `LoadMusicStream(...)` assignment in `app/session/play_session.cpp`.
- **Files changed:** `app/session/play_session.cpp`.
- **Validation evidence:** `cmake --build build -- -j4`; `./build/shapeshifter_tests "[song_playback]"`; `./build/shapeshifter_tests "[gamestate]"`; `./build/shapeshifter_tests "[play_session]"`; `./build/shapeshifter_tests "~[bench]"`; `git --no-pager diff --check` (all passed).
- **Decision logged:** #176 in `.squad/decisions.md` (2026-04-30T07:15:10Z)
- **Cross-team:** Baer added regression tests; Kujan approved full cycle.

### 2026-04-30 — Refactor: repeat intent moved to music-load seam

- **Refactor:** Added `app/audio/music_stream.h` with `load_music_stream(const char* path, bool repeat)` that wraps `LoadMusicStream(...)`, sets `stream.looping = repeat`, and returns the `Music`.
- **Call-site update:** `setup_play_session(...)` now calls `load_music_stream(path, false)` and no longer writes `music->stream.looping = false` directly.
- **Validation evidence:** `cmake --build build -- -j4`; `./build/shapeshifter_tests "[song_playback]"`; `./build/shapeshifter_tests "[gamestate]"`; `./build/shapeshifter_tests "[play_session]"`; `./build/shapeshifter_tests "[song_complete]"`; `./build/shapeshifter_tests "~[bench]"`; `git --no-pager diff --check` (all passed).

### 2026-04-30T03:05:46.543-07:00 — White full-lane wall was untinted bar model

- **Root cause:** LowBar/HighBar are full-width Model-authority obstacles. Their render path (`draw_owned_models`) ignored the obstacle `Color` component and drew with default material diffuse (white), producing a white wall across all three lanes.
- **Production fix:** In `app/systems/game_render_system.cpp`, `draw_owned_models` now queries `Color` and applies it to a local per-draw `Material` before `DrawMesh(...)`.
- **Behavior impact:** Obstacle behavior is unchanged (same spawn/collision/scoring); only rendering now matches intended bar colors instead of white.
- **Validation evidence:** `cmake --build build -- -j4`; `./build/shapeshifter_tests "[archetype]"`; `./build/shapeshifter_tests "~[bench]"` (all passed).
- **Regression coverage:** Added explicit LowBar/HighBar color assertions in `tests/test_obstacle_archetypes.cpp`.

## Session: White Lane Wall Fix (2026-04-30T10:05:46Z)

**Role:** Root cause identification and fix implementation

**Task:** Identify and remove white 3-lane wall visual issue while preserving obstacle behavior

**Work Summary:**
- Traced white wall visual to `draw_owned_models` path in `game_render_system.cpp`
- Root cause: `ObstacleModel` (owned model) render path was not applying `Color` component tint
- LowBar/HighBar obstacles use owned-model path, not `ModelTransform` shared path
- Fixed by adding `Color` to view and applying tint to material copy before `DrawMesh`

**Outcome:** Root cause identified and fixed; added archetype color regression assertions

**Status:** Complete

**Artifacts:**
- Decision: `.squad/decisions.md` (white lane wall fix section)
- Orchestration: `.squad/orchestration-log/2026-04-30T10-05-46Z-mcmanus.md`

## 2026-04-30T03:24:36.429-07:00 — Temporary bar disable (LowBar/HighBar)

- **Root cause:** Shipped charts still authored `low_bar`/`high_bar`, so bar entities continued to spawn and affect runtime paths.
- **Production fix:** `parse_beat_map` now drops `low_bar`/`high_bar` entries; `beat_scheduler_system` also skips `ObstacleKind::LowBar/HighBar` defensively.
- **Regression coverage:** Updated parser and scheduler tests to assert bar suppression; shipped difficulty ramp test asserts no bars in medium/hard loaded maps.
- **Follow-up balance adjustment:** Raised medium max silent-gap regression threshold from 32 to 34 beats while bars remain disabled.
- **Validation evidence:** `cmake --build build -- -j4`; targeted tags (`[low_high_bar]`, `[rhythm][beatmap]`, `[parse][kind]`, `[beat_scheduler]`); full `./build/shapeshifter_tests "~[bench]"` all passing.

---

## 2026-05-10T02:36:13-07:00 — Gameplay audit: onset timing consumption

**Task:** Audit gameplay loop, scoring, player/shape, collision, obstacle timing, and shipped beatmap consumption under `app/` and `tests/` without modifying code.

**Findings filed:**
- #404 — `beat_scheduler_system` ignores parsed `BeatEntry::time_sec`, so onset-authored obstacles are scheduled from `beat_times[beat]` instead of their per-entry onset time. Current shipped onset beatmaps show drift up to ~80 ms between `time_sec` and `beat_times[beat]`.

**Duplicate checks / not filed:**
- Runtime/load validation gap is already tracked by #128; CI shipped beatmap validation gap by #64.
- Cross-layer same-beat onset preservation in generation is already tracked by #385/#400; raw shipped onset metadata regression coverage by #398.

**Learning:** In the C++ runtime, `BeatEntry::time_sec` is parsed but currently not consumed by scheduling; obstacle arrival remains tied to snapped beat-grid timing unless #404 is fixed.

## 2026-05-10T02:40:52.785-07:00 — Fixed #404 + #395 gameplay runtime regressions

- **#404 runtime timing fix:** `beat_scheduler_system` now prioritizes authored `BeatEntry::time_sec` when metadata is explicitly present, and falls back to `beat_times[beat]`/grid timing when absent.
- **Parser contract update:** Added `BeatEntry::has_time_sec` and set it during beatmap parse so runtime can distinguish authored onset timing from fallback timing.
- **Regression tests:** Added scheduler tests for authored `time_sec` precedence and fallback behavior when `time_sec` is absent; parser tests assert `has_time_sec` true/false behavior.
- **#395 tuning fix:** Reduced `ENERGY_DRAIN_BAD` from `0.10f` to `0.05f` to restore early-player forgiveness.
- **Energy tuning tests:** Added explicit tests proving 5×Ok + 1×Bad is net-positive and that five Ok recoveries outweigh one Bad drain.
- **Validation:** Targeted issue tests passed; full validation passed with `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests`.

## 2026-05-10T02:40:52.785-07:00 — Round 2 gameplay/runtime audit (no code changes)

- Audited loop/scoring/player/collision/scheduling/beatmap-consumption paths after PR #408 with duplicate-first issue triage.
- Filed #423: LaneBlock `blocked_mask` invariant gap (parser/validator/runtime mismatch can block invisible lanes).
- Filed #424: Authored `time_sec` lacks monotonic/duration invariants, permitting misordered or never-spawned obstacles despite validation passing.
- Baseline validation: `ctest --test-dir build --output-on-failure` shows one pre-existing unrelated failure (`redfoot/#168: existing game_over buttons keep their original positions`).

## 2026-05-10 — Round 3 gameplay/runtime audit (no code changes)

- Scope: scheduler, collision, scoring, energy, session/game-loop invariants after PR #427.
- Duplicate-first triage: searched open issues before filing for same-beat ordering, song-finish stall, and post-finish scoring drift.
- Filed #442: unstable same-`beat_index` ordering can violate authored `time_sec` spawn semantics in scheduler consumption order.
- Filed #444: end-of-song invariant break can soft-lock Playing if `song.playing=false` before remaining obstacles clear.
- Filed #445: `scoring_system` continues passive distance/time accrual while `song.playing=false` in post-finish window.
- Validation context: baseline tests currently pass (`All tests passed (1954 assertions in 720 test cases)`).

## 2026-05-10T17:51:46.427-07:00 — Fixed #89 vertical GoEvent input

- Root cause: `player_input_handle_go` only consumed Left/Right lane deltas; Up/Down `GoEvent`s from keyboard, touch swipe, or test-player AI reached the dispatcher but never mutated `VerticalState`.
- Fix: Up now starts `VMode::Jumping` with `JUMP_DURATION`; Down now starts `VMode::Sliding` with `SLIDE_DURATION`; both are accepted only from `Grounded` so existing no-reset behavior remains intact.
- Regression coverage: Updated obsolete disabled-input assertions and added pipeline tests proving jump/slide activate through the normal `GoEvent` drain path.
- Validation: `VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests "[player],[player_rhythm],[input_pipeline]"`; `./build/shapeshifter_tests && git --no-pager diff --check` passed.
