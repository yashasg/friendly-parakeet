# Keaton — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** C++ Performance Engineer
- **Joined:** 2026-04-26T02:09:15.781Z

## Learnings

<!-- Append learnings below -->

### 2025 — Diagnostic Run 1

- **`on_construct<ScoredTag>` signal fires during `collision_system`** — at signal time `MissTag` is already present (emplaced just before ScoredTag), so `reg.any_of<MissTag>(entity)` is the correct miss detector inside signal handlers. The `transition_pending` heuristic only fires on fatal misses.
- **`std::rand()` is globally unseeded** — freeplay obstacle sequence is identical across runs. Tests seed it explicitly via `std::srand()`. The project uses `std::mt19937` elsewhere (`test_player_system`); that's the correct model.
- **`on_obstacle_destroy` is the main DOD hot spot** — O(n·m) linear scan over all `MeshChild` entities per destroyed obstacle. Worst case is `reg.clear()` at session start. Fix: reverse-lookup component (list of child entity IDs on the logical obstacle).
- **`validate_beat_map` Rule 1 guard `prev_beat >= 0` creates a blind spot** — first entry can have `beat_index < 0`, which produces a negative `beat_time` and causes immediate spawn at session start.
- **`scoring_system` removes `Obstacle`/`ScoredTag` from queried entities during `view.each()`** — safe in EnTT v3.16 (backward iteration), but fragile. Watch for regressions if EnTT version changes.
- **`PopupDisplay` emplaced in `scoring_system` is redundant** — `popup_display_system` immediately overwrites it with `emplace_or_replace`. The initial emplace is a wasted pool write.
- **`session_log.frame` is only updated by `test_player_system`** — in non-test-player sessions the frame counter always reads `0` in the log. Consider incrementing in `session_log_flush` instead.

### 2025 — Issue #231: ComboGate blocked_mask validation

**Scope:** `app/systems/beat_map_loader.cpp`, `tests/test_beat_map_validation.cpp`

- **Rule 5b added to `validate_beat_map`**: `ComboGate` entries with `blocked_mask == 0` (free points, no challenge) or `blocked_mask == 0x07` (unavoidable miss, all lanes blocked) are now rejected with explicit error messages.
- **Working tree was in a multi-branch partial-stash state** — untracked in-progress files (`test_audio_system.cpp`, `test_safe_area_layout.cpp`, `sfx_bank.cpp`, etc.) caused the full test build to fail. Work around by temporarily renaming untracked problem files before cmake configure, then restoring after build.
- **`git stash pop` conflict with `tools/rhythm_pipeline.py`** consumed a stash entry and caused edits to be lost mid-session. Always verify file contents after any `git stash` operation before proceeding.
- **`beat_map_loader.cpp` on `ecs_refactor` is a simplified version** — Rules 0, 0a, 5b, and several other rules present in the `main`/other branches are absent here. This branch has an earlier validator iteration.
- **Pre-existing test failures** in `[rhythm_helpers]` (`window_scale_for_tier`), `[scoring]`, and `[collision]` are unrelated to beatmap validation and existed before this change.
- **Shipped beatmaps contain no `combo_gate` entries** — new validation rule has zero impact on existing content.



**Scope:** Full pass over `app/**/*.cpp`, `app/**/*.h`, `CMakeLists.txt`. Cross-checked against all known issues #44–#162.

**Issues filed:**
- **#231** — `validate_beat_map` doesn't validate `ComboGate.blocked_mask` (0 = free points, 7 = unavoidable miss). Milestone: test-flight.
- **#232** — `save_settings` missing `file.flush()` before `file.good()` check. `save_high_scores` already does this correctly; inconsistency creates silent data-loss risk. Milestone: AppStore.
- **#233** — `size_hint() == 0` used as "player absent" guard in `collision_system`, `burnout_system`, `test_player_system`. Correct idiom is `begin() == end()`; `size_hint()` is a lower-bound hint per EnTT docs. Milestone: AppStore.
- **#234** — `popup_display_system` uses magic number `<= 3` to distinguish timing-grade vs numeric score popup. Should use the `255` sentinel constant. Milestone: AppStore.
- **#235** — `load_validation_constants()` opens `"content/constants.json"` CWD-relative only, no exe-relative fallback. Beatmap loading already uses a two-path fallback with `GetApplicationDirectory()`. Milestone: AppStore.

**Duplicates skipped:**
- COLLISION_MARGIN triplicated — already #85
- `std::rand` unseeded freeplay — already #108
- scoring_system mutates active view — already #123
- APPROACH_DIST duplicated in song_state — already #84
- validate_beat_map negative beat_index — already #132

**Observations:**
- `game_loop.cpp` seeds the `RNGState` mt19937 engine with `std::random_device` + clock entropy on init. The `RNGState{}` default seed of `1u` is only the in-struct default; the actual game seeds properly. Tests inject fixed seeds intentionally.
- Fixed-step loop runs `input_system`/`hit_test_system` once per frame outside the fixed tick accumulator. Double-processing of `EventQueue` across multiple fixed ticks in the same frame is benign due to state guards in `player_input_system`.
- `cleanup_system` runs in all non-Paused phases including `GameOver`/`SongComplete`. LanePush obstacles are always scored by `collision_system` (within `COLLISION_MARGIN`) before they reach `DESTROY_Y`, so no false miss penalty for off-lane LanePush.

### 2026-04-26 — Issue #236: haptic feedback pipeline

**Scope:** `app/components/haptics.h`, `app/systems/haptic_system.cpp`, `app/game_loop.cpp`, `app/systems/{burnout,player_input,player_movement,scoring,game_state}_system.cpp`, `tests/test_haptic_system.cpp`

- **Model `HapticQueue` exactly after `AudioQueue`** — fixed-size array with count + push/clear helpers. CMake `file(GLOB SYSTEM_SOURCES app/systems/*.cpp)` auto-picks up `haptic_system.cpp`; no CMakeLists changes needed.
- **Gate at push time, not consume time** — `haptic_push(HapticQueue&, bool, HapticEvent)` checks `haptics_enabled` before queuing. This lets tests verify suppression via `hq.count == 0` without needing a full system drain.
- **Null-safety pattern**: `!st || st->haptics_enabled` — when `SettingsState` absent from context, defaults to enabled (matches `haptics_enabled = true` field default). In production and tests, `SettingsState` is always present.
- **`SettingsState` was never emplaced in ECS context** — `ui_source_resolver.cpp` already used `reg.ctx().find<SettingsState>()` with null-safe pattern showing the intent. `game_loop_init` now emplaces it after loading from disk via `settings::load_settings()`.
- **Burnout zone → haptic mapping**: Only 3 of 5 Burnout events are fired. The 4 non-None zones are Risky=1.5×, Safe-buffer (none), Danger=3.0×, Dead=5.0×. `Burnout2_0x` and `Burnout4_0x` are defined but reserved — no zone boundary maps to them.
- **NearMiss definition**: Non-miss score while `burnout.zone == BurnoutZone::Dead` (survived the maximum danger zone). `MissTag` = failure, not near-miss.
- **`JumpLand` only, not takeoff** — spec says "Jump (land)". Haptic fires in `player_movement_system` on `Jumping → Grounded` transition, not at jump start in `player_input_system`.
- **`begin_shape_window` lambda** captures `reg` by reference; ShapeShift haptic fires inside it AND in the freeplay path after `audio_push(SFX::ShapeShift)`.
- **Pre-existing test failures** (`test_death_model_unified`, `test_high_score_integration`, `test_shipped_beatmap_shape_gap`) were confirmed pre-existing before this change and are unrelated.
- **`git stash pop` run twice in succession** overwrote three system files mid-session. Always check `git stash list` and pop cautiously. After any double-pop incident, verify file contents with `grep` before continuing.

### 2025 — Cleanup Pass 2

**Scope:** `app/**/*.cpp`, `app/**/*.h`, `app/util/`

**Changes shipped (commit f98732e):**

1. **`size_hint() == 0` → `begin() == end()`** in `collision_system.cpp`, `burnout_system.cpp`, `test_player_system.cpp`. Fixes issue #233. `size_hint()` is documented as a lower-bound hint in EnTT v3.16; using it as an empty-view guard is incorrect and can skip systems when the pool has phantom capacity.

2. **`ScorePopup::TIMING_TIER_NONE = 255` named constant** in `scoring.h`. Replaces bare `<= 3` magic in `popup_display_system.cpp` and `uint8_t{255}` in `scoring_system.cpp`. Fixes issue #234.

3. **`save_settings` flush before `file.good()`** in `settings_persistence.cpp`. Fixes issue #232. `save_high_scores` already did this; `save_settings` was the inconsistency that could silently drop buffered writes.

4. **`app/util/fs_utils.h`** — new shared header with `inline fs_utils::ensure_directory`. Eliminates identical duplicate that lived in both `settings_persistence.cpp` and `high_score_persistence.cpp` anonymous namespaces.

5. **C-style functional casts** — `float(constants::SCREEN_W)` / `float(constants::SCREEN_H)` replaced with `constants::SCREEN_W_F` / `constants::SCREEN_H_F` (new `constexpr float` companions added to `constants.h`). `float(SCREEN_W / 3)` replaced with `static_cast<float>(SCREEN_W / 3)`. Affected: `beat_scheduler_system.cpp`, `obstacle_spawn_system.cpp`, `ui_render_system.cpp`.

**Pre-existing test failures (not introduced):**
- `test_high_score_integration` (SIGABRT from `dense_map.hpp` assert) — pre-existing.
- `test_collision_system` (2 timing-window tests) — pre-existing; BAD/Perfect `window_scale` logic mismatch with test expectations.
- `test_shipped_beatmap_*` variants — pre-existing content-level failures, unrelated to C++ cleanup.

**Working-tree observation:** Multiple agents leave untracked/unstaged files in the shared working tree. `git stash` can pick up and reintroduce these mid-session; always verify file contents after any stash operation. Committed only files I owned; reset `collision_system.cpp` and `ui_render_system.cpp` to HEAD before re-applying my hunks to avoid bundling other agents' work.

## Learnings

### PR #43 Review Fixes (2026-04-27)

Fixed all 7 unresolved review threads in commit d90abf9 on `user/yashasg/ecs_refactor`.

1. **ScreenTransform stale on first/resize frame** — extracted `compute_screen_transform()` helper from `ui_camera_system` into a free function declared in `camera_system.h`; called before `input_system` in `game_loop_frame`.

2. **slab_matrix obstacle Y/Z swap** — LanePush/LowBar/HighBar direct transforms: `OBSTACLE/LOWBAR/HIGHBAR_3D_HEIGHT` now maps to Y (vertical), `DrawSize.h` maps to Z (scroll depth).

3. **MeshChild slab Y/Z swap** — `slab_matrix(mc.x, z, mc.width, mc.height, mc.depth)` — mc.height→Y, mc.depth→Z.

4. **level_select_system early return skips diff button reposition** — extracted `update_diff_buttons` lambda; called after Up/Down GoEvent changes `selected_level`, and after SelectLevel button press.

5. **Title screen Confirm/Exit hitbox overlap at EXIT_TOP** — Confirm half-height = `(EXIT_TOP - 1.0f) / 2.0f`; regions are now [0, EXIT_TOP-1] and [EXIT_TOP, EXIT_BOTTOM].

6. **on_obstacle_destroy O(N²) scan** — added `ObstacleChildren` component to `rendering.h`; `add_slab_child`/`add_shape_child` register children via `get_or_emplace<ObstacleChildren>(parent).push(e)`; `on_obstacle_destroy` now O(count) direct destroy.

7. **Paused overlay reloads paused.json every frame** — cache detected via `ui.active != ActiveScreen::Paused`; file open+parse only on first entry; subsequent frames set `ui.has_overlay = true` and reuse `ui.overlay_screen`.

**Validation:** `cmake --build build --parallel` → zero warnings; `./build/shapeshifter_tests` → 2067 assertions, 667 test cases, all pass.

### 2026-05-17 — DECLARE_ENUM Macro Design

**Context:** User asked to replace enums with a macro that takes the enum name as input. Fixed-7-arg unscoped macro was rejected by Keyser/Kujan. X-macro `FOO_LIST(X)` pattern was rejected by user as too cumbersome. Design pass only; no code changed.

**Key learnings:**

- **C++ preprocessor hard limit:** You cannot `#define` a macro from inside another macro expansion. This means a macro cannot generate a `FOO_LIST` internally — the list must be supplied inline as variadic args at the call site. No workaround in standard C++.
- **Reflection limit:** The preprocessor (and C++20 without magic_enum/libclang) cannot derive enumerator names from an `enum class` that is already defined by name alone. The list must be at the definition site.
- **`__VA_OPT__` (C++20) enables clean FOR_EACH:** The recursive deferred expansion pattern (`_SE_PARENS`, `_SE_EXPAND`, `_SE_MAP_H`, `_SE_MAP_AGAIN`) handles variable arity without counting macros or BOOST_PP. The project uses C++20 (`set(CMAKE_CXX_STANDARD 20)`), so `__VA_OPT__` is available on all target compilers (clang, MSVC 2019+, emscripten).
- **`inline const char* ToString()` + `constexpr` array = zero ODR, zero allocation:** The macro generates an inline function with a `static constexpr const char* const _s[]` inside. The array lives in `.rodata`; `inline` ensures a single definition across TUs in C++17+.
- **Explicit values stay manual:** Enums with `= N` annotations (GamePhase, EndScreenChoice, BurnoutZone, WindowPhase, Layer, VMode, FontSize) should keep explicit values — they document ABI/protocol intent. Adding tuple-unpacking macro support for `(Name, Value)` pairs costs 3–4 extra helpers for 5 enums; not worth it.
- **7 of 23 enums convert; 16 stay manual:** Shape, ObstacleKind, TimingTier (replace X-macro), SFX, HapticEvent, ActiveScreen, DeathCause (add ToString). All others are pure-data or have explicit values.
- **WindowPhase forward-decl works fine:** `DECLARE_ENUM` generates a definition in `rhythm.h`. `player.h` keeps its one-line `enum class WindowPhase : uint8_t;` forward decl. No regression.
- **death_cause_to_string in ui_source_resolver.cpp is the only logic migration** — it becomes `ToString(DeathCause)` once that enum converts.
- **Do not start until PR #43 merges** — target headers (player.h, rhythm.h, obstacle.h, audio.h, haptics.h, ui_state.h) are all in the PR #43 conflict zone.

### 2026-05-17 — magic_enum ToString Refactor (commit 8fbce9c)

**Scope:** `app/components/player.h`, `app/components/obstacle.h`, `app/components/rhythm.h`, `app/components/audio.h`, `app/systems/sfx_bank.cpp`, `tests/test_audio_system.cpp`

- **X-macro scaffolding fully removed** — SHAPE_LIST, OBSTACLE_KIND_LIST, TIMING_TIER_LIST macros replaced with plain `enum class` declarations. ToString() now delegates to `magic_enum::enum_name()`, no generated switch.
- **magic_enum::enum_name().data() is safe for printf** — In magic_enum 0.9.7, `enum_name_v<E, V>` is a `static constexpr static_str<N>` with `chars_{..., '\0'}`. The string_view it returns points to null-terminated static storage. Using `.data()` for `%s` is correct and verified from the header source.
- **SFX::COUNT removed** — Count derived via `magic_enum::enum_count<SFX>()`. SFXBank::SFX_COUNT and the local SFX_COUNT in sfx_bank.cpp both use enum_count. A `static_assert` ties SFX_SPECS.size() to enum_count so any future SFX addition without a corresponding spec entry fails at compile time.
- **Test edit justified** — `test_audio_system.cpp` used `SFX::COUNT` in a static_assert and in a modulo expression; both replaced with `magic_enum::enum_count<SFX>()`. This was the minimum edit to unblock compile.
- **Validation:** zero warnings, 2123 assertions, 667 test cases, all pass.
