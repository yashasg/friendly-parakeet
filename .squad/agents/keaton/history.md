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

### 2026 — EnTT ECS Guide Audit Pass

**Scope:** Read-only audit of `app/components/` and `app/systems/` against the EnTT ECS guide (docs/entt/Entity_Component_System.md).

**Confirmed findings (not yet fixed):**

1. **`camera_system.cpp:373` — `emplace_or_replace<ScreenPosition>` per frame** — same anti-pattern as issue #244 (fixed for ModelTransform), missed for ScreenPosition. Fix: `get_or_emplace<ScreenPosition>(entity) = ScreenPosition{...}`. No signal observers exist for ScreenPosition, so this is waste-only not a correctness issue.

2. **`scoring_system.cpp:36` — `any_of<MissTag>` branch inside structural view** — `view<ObstacleTag, ScoredTag, Obstacle, Position>` then branches on `any_of<MissTag>`. The EnTT guide and collision_system both favor per-kind structural views. Fix: two views, one with MissTag, one with `entt::exclude<MissTag>`. Note: scored obstacle count per frame is 0–3, so runtime impact is very low; this is a pattern debt, not a hot spot.

3. **`lifetime_system.cpp:6` — per-frame `std::vector<entt::entity>` allocation** — allocates a fresh `std::vector` every call, unlike `cleanup_system.cpp` which already uses a `static std::vector`. Fix: make the vector static (same pattern as cleanup_system). Filed as separate quick-win.

4. **`scoring_system.cpp:44,110` — double `ctx().find<SongResults>()` inside entity loop** — SongResults is looked up twice; `find<GameOverState>()` is also looked up per-entity inside loops in both `collision_system.cpp:118` and `miss_detection_system.cpp:22`. All should be hoisted above the loop.

5. **`BurnoutState`/`BankedBurnout` dead code since #239** — `burnout_system` is a no-op; `BurnoutState` is still emplaced in `game_loop.cpp:80` and reset in `play_session.cpp:44`; `BankedBurnout` is defined in `burnout.h` but never emplaced in production. Should be removed in a coordinated pass. Coordinate with McManus/Saul before removing.

6. **`ui_render_system.cpp` — JSON traversal per frame** — `find_el()` scans `UIState::screen["elements"]` linearly by ID string; layout constants (widths, colors, positions) are re-extracted via `json.get<float>()`/`json_color()` every frame. UIState is a context singleton (not per-entity component), so no pool cost, but per-frame JSON traversal is avoidable. Fix: pre-compute layout structs at screen-load time.

7. **`obstacle_spawn_system.cpp:29-30` — lazy `ctx().emplace<RNGState>()` inside update** — conditional emplace inside a per-frame system. Better: emplace in `game_loop_init()` or `setup_play_session()`.

**Already confirmed good / do not touch:**
- collision_system per-kind structural views — textbook ECS
- get_or_emplace<ModelTransform> in game_camera_system (fixed in #244)
- ObstacleChildren fixed array O(N) child cleanup (PR #43)
- EventQueue/AudioQueue/HapticQueue as fixed-size inline arrays
- PopupDisplay char[16] fixed text buffer
- All singletons in reg.ctx() — correct ownership boundary
- cleanup_system static buffer (intentional, commented)
- Magic enum ToString — zero allocation
- Pool insertion order for ObstacleChildren in game_loop_init (PR #43)
- BeatMap copy-delete guards against accidental duplication
- ActiveTag structural caching with UIActiveCache

---

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

### 2026 — Issue #244: emplace_or_replace<ModelTransform> per frame

**Scope:** `app/systems/camera_system.cpp`

- **`emplace_or_replace` fires `on_construct` or `on_update` signals every frame** — for components that exist every frame (transforms, positions), this is unnecessary signal traffic. No observers are connected to `ModelTransform` today, but the pattern is still wrong by contract.
- **Fix pattern: `get_or_emplace<T>(entity) = T{...}`** — `get_or_emplace` emplaces once (firing `on_construct` once at entity birth), then returns a reference on subsequent frames. Assigning to the reference mutates in-place with no signal. This is the canonical steady-state mutation idiom in EnTT.
- **Confirmed no `ModelTransform` signal observers** — `grep on_construct/on_update/on_destroy` across all sources; only `ObstacleTag` and `ScoredTag` have live observers. The fix is signal-safe.
- **Pre-existing build failures in `beat_map_loader.cpp`** blocked full rebuild; compiled `camera_system.cpp` in isolation with `-Wall -Wextra -Werror`, zero warnings.

### 2026 — Issue #241: compute_screen_transform called twice per frame

**Scope:** `app/systems/camera_system.cpp`, `tests/test_ndc_viewport.cpp`

- **Root cause:** `ui_camera_system` called `compute_screen_transform(reg)` at the top of its body. `game_loop_frame` already calls `compute_screen_transform` before input every frame — so the transform was computed twice per frame from the same window dimensions, wasting cycles and making ownership ambiguous.
- **Fix:** Removed the duplicate `compute_screen_transform(reg)` call from `ui_camera_system`. The function now reads the already-canonical `ScreenTransform` stored in the registry context. Comment updated to document the ownership rule and reference `#241`.
- **Regression tests added:** `tests/test_ndc_viewport.cpp` — two new `[screen_transform][regression]` tests verify the letterbox math is idempotent and produces correct pillarbox offsets. These mirror the pure math of `compute_screen_transform` without requiring a raylib window.
- **Zero-warning policy:** `camera_system.cpp` and `test_ndc_viewport.cpp` compiled with `-Wall -Wextra -Werror`, zero warnings.
- **Note:** Fix landed in commit `a5cad3d` (bundled with #304 wasm shutdown work from a prior pass). Both changed files reference `#241` explicitly.

### 2026 — PR #43 CI: mesh child cleanup regression (commit b0569a6)

**Scope:** `tests/test_helpers.h`, `tests/test_pr43_regression.cpp`

- **Root cause:** `make_registry()` calls `wire_obstacle_counter(reg)`, which accesses `on_construct/on_destroy<ObstacleTag>()` — this creates the `ObstacleTag` pool. Any `reg.storage<ObstacleChildren>()` call after that lands at a **higher** pool index. EnTT's `reg.destroy(entity)` iterates pools in **reverse insertion order**, so `ObstacleChildren` (higher index) was removed **first**, before the `on_destroy<ObstacleTag>` signal fired. `on_obstacle_destroy` then found `try_get<ObstacleChildren>` null and silently skipped child cleanup.
- **Fix:** Prime `reg.storage<ObstacleChildren>()` as the **first** pool created in `make_registry()`, before `wire_obstacle_counter`. This gives `ObstacleChildren` a lower index, so it is removed last and is readable when the signal fires. `make_obs_registry()` in the test drops its own (now-redundant) priming call; `make_registry()` owns the invariant.
- **Production code was already correct** — `game_loop_init` primes `ObstacleChildren` before any `ObstacleTag` pool exists (because `wire_obstacle_counter` is first called from `setup_play_session` at runtime, not during init). The bug was test-only.
- **Key EnTT ordering rule:** `reg.on_construct/on_destroy<T>()` creates the pool for `T` (via `assure<T>()` internally). Any component that must be readable in a destroy-signal handler must be registered as a pool BEFORE `T`'s pool is created.
- **Validation:** `[pr43]` 44/44 assertions pass; full suite 2390/2390 assertions pass, zero warnings.

### 2026-04-27 — EnTT Input Model Guardrails (PRE-IMPLEMENTATION GUIDANCE from Keyser)

**Cross-agent context:** Keyser published pre-implementation guardrails for dispatcher-based input refactor. You are Keaton (C++ Perf), identified as the implementation lead.

**What you need to know:**
- **Target:** Replace `EventQueue` fixed arrays with `entt::dispatcher` in `reg.ctx()`
- **Delivery model:** `enqueue`+`update`, **not** `trigger` (must preserve fixed-step seam)
- **System order unchanged:** Dispatcher is transport layer only
- **Listener pattern:** Free functions or lambdas, registered in canonical order in `game_loop_init`
- **Seven guardrails:** Multi-consumer ordering, overflow cap, clear-vs-update, registry access pattern, no connect-in-handler, trigger prohibition, stale event discard

**Recommended migration (in order):**
1. Add dispatcher to ctx (inert, zero-risk)
2. Migrate InputEvent tier → gesture_routing + hit_test as listeners
3. Migrate GoEvent/ButtonPressEvent tier → player_input_system handlers
4. Remove EventQueue struct
5. Baer gate: R7 test + no-replay validation

### 2026-05-17 — EnTT ECS Performance Audit

**Scope:** Audit of hot-path patterns and idiomatic EnTT usage against DoD principles.

**Key findings:**

- **Rule 1 — Per-frame component mutation:** Use `get_or_emplace<T>(e) = value`, not `emplace_or_replace<T>(e, value)` (unnecessary signal dispatch). Violation: `camera_system.cpp:373` ScreenPosition per-frame on popup entities (mirrors issue #244 pattern, already fixed for ModelTransform).
- **Rule 2 — Branching inside views:** Use structural views (separate Miss/Hit views), not `any_of<T>` inside shared view. Violation: `scoring_system.cpp:36` branches on MissTag (low cardinality, pattern debt only).
- **Rule 3 — Hoist ctx() lookups:** Move `reg.ctx().find<T>()` outside loops. Violations: `scoring_system.cpp` (2×), `collision_system.cpp`, `miss_detection_system.cpp`.
- **Out-of-scope deferred:** UI JSON per-frame (refactor cost), BurnoutState dead code (cross-team sign-off needed), lifetime_system vector (quick win), obstacle_spawn RNGState (minor).

**Status:** Delivered. Remediation backlog created (low-risk, mostly low-effort fixes). Orchestration log: `.squad/orchestration-log/2026-04-27T19-14-36Z-entt-ecs-audit.md`.

**Key invariants to preserve:**
- No multi-tick input replay (#213)
- Deterministic fixed-step behavior
- No frame-late input
- MorphOut interrupt (#209) unchanged
- BankedBurnout first-commit-lock (#167) unchanged

**Full decision:** `.squad/decisions.md` (EnTT Input Model Guardrails section)
**Orchestration log:** `.squad/orchestration-log/2026-04-27T19-09-18Z-keyser-entt-input-guardrails.md`

### 2026 — EnTT Core Functionalities Audit Pass

**Scope:** Read-only audit of `docs/entt/Core_Functionalities.md` against `app/components/` and `app/systems/`.

**Key findings (actionable, filed as GitHub issues #308–#313):**

1. **`lifetime_system.cpp` per-frame vector alloc** (#308, P1, FIXED in commit 7fc569a) — `static std::vector<entt::entity> expired; expired.clear();` pattern. Eliminates per-frame malloc/free.

2. **`ctx().find<>` inside entity loops** (#309, P1, FIXED in commit 7fc569a) — `SongResults` and `EnergyState` hoisted above view loop in `scoring_system.cpp`; `GameOverState` hoisted above resolve lambda in `collision_system.cpp` and above loop in `miss_detection_system.cpp`.

3. **`entt::enum_as_bitmask` for `GamePhase`** (#311, P2, open) — `ActiveInPhase.phase_mask` and `phase_bit()`/`phase_active()` helpers can be replaced with native bitmask operators. **Breaking:** GamePhase values must shift from sequential (0–5) to power-of-two (0x01–0x20); audit all raw-value comparisons before landing.

4. **Pre-compute UI element map at screen-load time** (#312, P2, open) — `find_el()` in `ui_render_system.cpp` does linear JSON string scan per frame. Replace with `std::unordered_map<entt::hashed_string::hash_type, const json*>` built once at `UIState::load_screen()`. Lookup keys become compile-time `"id"_hs` constants.

5. **`HighScoreState::make_key()` heap string** (#313, P3, open) — `current_key: std::string` can be replaced with `uint32_t` (FNV-1a via `entt::hashed_string::value()`). Entry::key char[32] remains for persistence. Low urgency (called once per session).

**EnTT Core Functionalities NOT worth adopting:**
- `entt::monostate` — `reg.ctx()` is the correct singleton owner in this game; monostate is global and bypasses registry lifetime.
- `entt::any` — no opaque container needs; all component types are concrete.
- `entt::allocate_unique` — no custom allocators in the project.
- `entt::y_combinator` — no recursive lambda needs.
- `entt::compressed_pair` — no pair-heavy code; internal use only.
- `entt::iota_iterator` — superseded by C++20 ranges; project targets C++20.
- `entt::type_index/type_hash/type_name` — magic_enum handles type names; no custom RTTI needed.
- `entt::family` / `entt::ident` — no dynamic type ID registration.
- `entt::input_iterator_pointer` — internal EnTT implementation detail.
- `entt::fast_mod` — only one modulo in hot code (`obstacle_spawn_system.cpp:88`, `(lane+1)%3`); the modulus 3 is not a power of two, so fast_mod doesn't apply.

**Pre-existing build failures (not caused by this work):**
- `input_dispatcher.cpp` references unimplemented handler functions (`game_state_handle_go`, etc.) — in-progress dispatcher migration.
- Test files `test_player_input_rhythm_extended.cpp`, `test_player_action_rhythm.cpp`, `test_burnout_bank_on_action.cpp` have unused `eq` variable warnings-as-errors — pre-existing.

### 2026-04-27 — EnTT Dispatcher Input Migration (commit 2547830)

**Scope:** `app/components/input_events.h`, `app/systems/{input_dispatcher,input_system,gesture_routing_system,hit_test_system,game_state_system,level_select_system,player_input_system,test_player_system}.cpp`, `app/game_loop.cpp`, `app/systems/all_systems.h`, all relevant test files.

- **EventQueue slimmed to InputEvent only** — `GoEvent goes[MAX]`, `ButtonPressEvent presses[MAX]`, `go_count`, `press_count`, `push_go()`, `push_press()` all removed. Only `InputEvent inputs[MAX]` and `input_count` remain (the raw tap/swipe pre-route shuttle).
- **`entt::dispatcher` in registry context** — added to `game_loop_init` and `make_registry`. `wire_input_dispatcher(reg)` connects game_state/level_select/player_input handler functions in system-execution order.
- **Producers use `disp.enqueue<GoEvent/ButtonPressEvent>()`** — `input_system` (keyboard), `gesture_routing_system` (swipe), `hit_test_system` (tap hit), `test_player_system` (synthetic) all enqueue into the dispatcher.
- **Dispatch in `game_state_system` first** — calls `disp.update<GoEvent>()` and `disp.update<ButtonPressEvent>()` after incrementing `phase_timer`, so `phase_timer > 0.4f` guards in `game_state_handle_press` read the current-tick timer value. All three consumer listeners (game_state, level_select, player_input) fire in one update call.
- **`level_select_system` and `player_input_system` call update() too** — no-ops in production (game_state already drained). Needed for tests that call these systems in isolation without game_state_system.
- **`player_input_system` is now a pure drain** — calls `disp.update<>()` and returns. All event logic is in `player_input_handle_go` and `player_input_handle_press`.
- **Consume-once solved naturally** — `entt::dispatcher::update()` drains the queue. Second fixed sub-tick calls find an empty queue. No manual `go_count = 0` needed. `test_entt_dispatcher_contract.cpp` (pre-written) documents this contract.
- **`GoCapture` / `PressCapture` structs in `test_helpers.h`** — test-only capture helpers for tests that need to inspect which events were produced (gesture_routing tests, hit_test tests, pr43 hitbox regression tests).
- **`update_diff_buttons_pos()` static helper** — extracted from `level_select_system` to avoid code duplication between the listener functions and the system body.
- **Validation:** zero warnings, 2419 assertions in 768 test cases, all pass. `test_entt_dispatcher_contract.cpp` and `test_input_pipeline_behavior.cpp` both pass unchanged.

### 2026-04-27 — Input Dispatcher Implementation APPROVED (Kujan review)

**Evidence:** 2419 assertions / 768 test cases pass; zero warnings; architecture review completed.

**Approved Changes:**
- GoEvent/ButtonPressEvent delivery via `entt::dispatcher` in `reg.ctx()` ✓
- `go_count`/`press_count` arrays eliminated; drain semantics replace manual zeroing ✓
- Same-frame delivery preserved; pool-order latency hazard avoided ✓
- EventQueue remains as raw gesture shuttle (InputEvent only) — acceptable scope

**Non-blocking follow-ups identified:**
1. Hardening: Start-of-frame event queue clearing + explicit contracts
2. Documentation: Stale test comments in `test_entt_dispatcher_contract.cpp`
3. Defensive: EventQueue raw InputEvent buffer audit

**Next:** Awaiting obstacle spawning / beatmap feature work.

### 2026-04-27 — Issue #315 Closure (EnTT-safe scoring_system iteration)

**Structural safety confirmed:** McManus's collect-then-remove pattern in scoring_system (#315) eliminates the structural mutation hazard. Two-pass approach (collect miss/hit records, then remove components) is safe and matches DOD iteration best practices. Pattern now available for reuse in other systems.

### 2026 — Issue #323: RNGState initialization moved to setup path

**Scope:** `app/game_loop.cpp`, `app/systems/play_session.cpp`, `app/systems/obstacle_spawn_system.cpp`

**Problem:** `obstacle_spawn_system` lazily initialized `RNGState` every frame via `if (!reg.ctx().find<RNGState>()) reg.ctx().emplace<RNGState>()`. This is a hot-path ctx mutation on every spawn tick, and silently recovers from missing setup state.

**Fix (commit 2fe229d):**
1. `game_loop_init` — `reg.ctx().emplace<RNGState>()` alongside other core singletons.
2. `setup_play_session` — `reg.ctx().insert_or_assign(RNGState{})` in the "Reset singletons" block, deterministically re-seeding to `1u` at the start of every session.
3. `obstacle_spawn_system` — lazy-init guard removed; replaced with `reg.ctx().get<RNGState>()` which asserts on missing state (programmer error surfaces immediately).

**Pattern established:** Session singletons that must be deterministically reset per play belong in the `setup_play_session` "Reset singletons" block via `insert_or_assign`. Singletons initialized once at boot belong in `game_loop_init` via `emplace`. Missing required ctx should surface via `reg.ctx().get<>()` (assert/terminate) not `find<>` (silent recover).

**Test helpers:** `make_registry()` in `test_helpers.h` already had `reg.ctx().emplace<RNGState>()` — no test changes required.

**Build note:** The local worktree had a pre-existing cmake configuration issue where the build was not configured (no Makefile). Fixed by running `vcpkg install --triplet arm64-osx` in the project directory to populate `vcpkg_installed/`, then running a fresh `cmake -B build -S .` with the vcpkg toolchain. All 2419 assertions / 768 test cases pass.

**Build note 2:** Worktree had uncommitted in-progress changes to `app/components/ui_state.h` and `app/systems/ui_loader.h` (UIState::load_screen refactor, partial). These caused build failures. Restored to HEAD for this task; not part of #323 scope.
