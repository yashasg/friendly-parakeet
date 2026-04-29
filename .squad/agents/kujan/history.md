# Kujan — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Reviewer
- **Joined:** 2026-04-26T02:12:00.632Z

## 2026-04-29 — Review: Vendored raygui Removal (User Directive Compliance)

**Implementer:** Hockney  
**Issue:** User directive — do not commit vendored raygui when vcpkg provides it at build time

**Scope:** Verified complete removal of `app/ui/vendor/raygui.h`, vcpkg integration via `vcpkg.json` and CMake, all includes switched to system `<raygui.h>`, single RAYGUI_IMPLEMENTATION definition retained in `app/ui/raygui_impl.cpp`.

**Tests run:**
- Full non-bench suite: **867 test cases, 2603 assertions — all pass**
- Build: zero warnings (arm64 macOS clang, -Wall -Wextra -Werror)

**Verification checklist:**
| Requirement | Outcome |
|---|---|
| `app/ui/vendor/raygui.h` removed | ✅ Absent, zero references in source/CMake |
| `vcpkg.json` lists `raygui` | ✅ Present in dependencies |
| CMake `find_path(RAYGUI_INCLUDE_DIR raygui.h REQUIRED)` as SYSTEM | ✅ Applied on `shapeshifter_lib` |
| `raygui_impl.cpp` is project-owned glue only | ✅ Minimal TU with single `#define RAYGUI_IMPLEMENTATION` |
| Exactly one `RAYGUI_IMPLEMENTATION` definition | ✅ Only in `app/ui/raygui_impl.cpp` |
| All screen controllers use `<raygui.h>` (system include) | ✅ Confirmed across 8 controllers |
| `ui_render_system.cpp` clean (no adapters/JSON/ECS loops) | ✅ Verified |
| Generated layout headers exclude RAYGUI_IMPLEMENTATION | ✅ Confirmed |

**Non-blocking follow-ups (stale doc language):**
1. `tools/rguilayout/SUMMARY.md` lines 76–78, 81–88, 237 — update "future work" status to ✅ Resolved with pointer to this change
2. `design-docs/rguilayout-portable-c-integration.md` line 283 — change example `#include "raygui.h"` to `#include <raygui.h>` to match actual implementation

**Reusable quality note:** When completing a "future build-integration task" documented as deferred in earlier reports, update the status in all referenced summary/integration docs simultaneously. Stale "Future task" bullets become materially misleading for the next developer.

**Verdict:** ✅ APPROVED WITH NOTES

---

## 2026-05-17 — Review: issues #311/#314 (GamePhaseBit enum with entt::enum_as_bitmask)

**Scope:** Keaton implementation — `GamePhaseBit` power-of-two enum class with `_entt_enum_as_bitmask` sentinel in `game_state.h`; `GamePhase` stays sequential state-machine discriminant; `to_phase_bit(GamePhase)` bridge function; `ActiveInPhase::phase_mask` typed as `GamePhaseBit`; all `phase_bit()`/raw `uint8_t` spawn sites migrated; 5 new `[phase_mask]` tests (28 assertions).

**Commit reviewed:** 6efd1b7b1d02cd19428c6e4f0286fa15c145d09e

**Merge compatibility:** Branch is 1 commit ahead of merge base `b5e81c1`. Target (`origin/user/yashasg/ecs_refactor`) has since integrated #312 (`e04d35b`) and #318 (`fdcd709`). `git merge-tree` reports zero conflicts — disjoint file sets (UI element map, high-score/settings vs. phase bitmask changes).

**Tests run:**
- Focused `[phase_mask]`: **28 assertions in 5 test cases — all pass**
- Full suite: **2616 assertions in 813 test cases — all pass**
- Build: zero warnings (arm64 macOS clang, -Wall -Wextra -Werror)

**Findings:**

**Correct work (all AC met):**
- AC1: `_entt_enum_as_bitmask` sentinel is last in `GamePhaseBit`, enabling EnTT typed `|/&/^` operators. Correct usage.
- AC2: `GamePhase` untouched (sequential 0–5), no switch/comparison sites at risk. `to_phase_bit` bridge is `constexpr`, `[[nodiscard]]`, maps each `GamePhase` value to the correct power-of-two bit. Round-trip test validates all 6 values.
- AC3: `ActiveInPhase::phase_mask` is `GamePhaseBit` (not `uint8_t`). Zero raw `phase_bit()` call sites remain in production code. `test_helpers.h` uses `to_phase_bit()` at the one remaining bridge site.
- AC4: Components (`ActiveInPhase`, `GameState`) are data-only. `phase_active()` is a free helper in the header, not a member method — no registry logic in component headers.
- AC5: Tests cover single-phase (6 assertions), multi-phase OR (6 assertions), distinct-bit uniqueness, round-trip all 6 phases, empty-mask semantics. Bit collision is explicitly tested via the combined-mask inequality assertions.

**Non-blocking notes:**
- `to_phase_bit` is not bounds-checked for hypothetical future `GamePhase` values ≥ 8 (would shift off `uint8_t`). Acceptable for current 6-phase design; a future addition should add a `static_assert` guard.
- `GamePhaseBit{}` default value is 0 (no named enumerator), which is valid C++ and semantically correct (empty mask).

**Reusable quality note:** When using `_entt_enum_as_bitmask`, keeping the state-machine enum (sequential) separate from the bitmask enum (power-of-two) avoids mixed-use bugs. Bridge via `constexpr` conversion function is the correct pattern.

**Verdict:** ✅ APPROVED — clean merge, zero warnings, all tests pass, all 5 acceptance criteria met.

### 2026-05-17 — Review: issue #313 (replace std::string current_key with entt::hashed_string)

**Scope:** Keaton implementation — `880b389 perf(#313): replace std::string current_key with entt::hashed_string hash`

**Commits reviewed:** 880b389

**Diff summary (vs `origin/user/yashasg/ecs_refactor`):**
- `high_score.h`: removed `current_key` (std::string) and `make_key()`; added `current_key_hash` (uint32_t), `make_key_str()`, `make_key_hash()`, `ensure_entry()`, `get_score_by_hash()`, `set_score_by_hash()`, `get_current_high_score()`
- `high_score_persistence.cpp`: `update_if_higher` uses `get_score_by_hash`/`set_score_by_hash`; preserves `current_key_hash` across load
- `play_session.cpp`: uses stack buffer + `ensure_entry` + `current_key_hash` instead of heap string
- Tests: 5 new test cases replacing `make_key()` string tests with hash stability, collision, key_str format, update-never-lowers, and persistence-preserves-session-key tests

**Acceptance Criteria:**
- AC1 ✅: No heap std::string current-key storage; `current_key_hash` is uint32_t
- AC2 ✅: `static_cast<const char*>` forces runtime overload explicitly; comment documents why
- AC3 ✅: Persistence uses `char key[KEY_CAP]` entries serialized as JSON strings — round-trip stable
- AC4 ✅: `update_if_higher`, `get_current_high_score` behavior unchanged except representation
- AC5 ✅: 16 test cases / 85 assertions in `[high_score]`; all pass
- AC6 ✅: `git merge-tree` confirms clean auto-merge; `play_session.cpp` "changed in both" resolves cleanly (burnout+GamePhaseBit edits vs hash wiring are disjoint sections)

**Tests run:**
- Focused `[high_score]`: **85 assertions / 16 test cases — all pass** ✅
- Full suite: **2645 assertions / 815 test cases — all pass** ✅
- Build: zero warnings ✅

**Merge compatibility:** Branch is 1 commit ahead, 3 commits behind `origin/user/yashasg/ecs_refactor`. `play_session.cpp` "changed in both" — auto-merge is clean (burnout removal + GamePhaseBit from ecs_refactor and hash wiring from #313 touch disjoint lines). 5 burnout files "removed in remote" — no conflict since #313 doesn't touch them.

**Non-blocking notes:**
- `<string>` include in `high_score.h` is still needed for `HighScorePersistence::path` — correct scope
- `get_score_by_hash` re-hashes stored keys on every lookup (O(N × key_len)); negligible for N ≤ 9

**Reusable quality note:** When replacing a heap type (std::string) with a hash for a session-scoped field, the key pattern is: (1) `ensure_entry()` at session start to pre-register the string entry, (2) `current_key_hash` for zero-allocation in-session updates, (3) preserve hash across persistence loads. This decouples allocation from the hot update path while keeping stable string persistence.

**Verdict:** ✅ APPROVED

## Learnings

### 2026-05-17 — Review: issue #322 (precompute UI layout data into POD cache structs)

**Scope:** Keyser implementation — `HudLayout` and `LevelSelectLayout` POD structs in new `ui_layout_cache.h`; builder functions `build_hud_layout()` / `build_level_select_layout()` added to `ui_loader.cpp`/`ui_loader.h`; cache stored via `reg.ctx().insert_or_assign()` in `ui_navigation_system` on screen change; `ui_render_system` draw paths consume POD cache with no JSON access for HUD/level-select stable constants; 11 tests in `test_ui_layout_cache.cpp`.

**Commit reviewed:** f8e049a

**Merge compatibility:** `git merge-tree origin/user/yashasg/ecs_refactor HEAD` returns a single clean tree hash — zero conflicts. Disjoint file set (new file `ui_layout_cache.h`, additions to `ui_loader`, `ui_navigation_system`, `ui_render_system`, new test file; no overlap with #312/#318/#311/#314 changes).

**AC verification:**
- AC1 ✅ Render path: `find_el`, `json_color`, `element_map` — all removed from `ui_render_system.cpp`. Confirmed with grep. Remaining `overlay_screen` JSON access is for transient per-load overlay color, correctly out of scope.
- AC2 ✅ POD structs are plain data. Built at screen-change boundary (`ui_navigation_system` after `build_ui_element_map`). Stored in `reg.ctx()` consistent with existing singleton pattern.
- AC3 ✅ Render output is pixel-equivalent (same multiplications, same field names, same fallback values `corner_radius=0.1f/0.2f`). No behavioral delta.
- AC4 ✅ 11 tests: 3 invalid-input cases for HudLayout (empty, missing element, missing nested field), 2 valid/construction cases, 1 integration against shipped `gameplay.json`; 3 invalid-input cases for LevelSelectLayout (empty, missing song_cards, missing difficulty_buttons), 1 valid/construction, 1 integration against shipped `level_select.json`. All 64 assertions pass.
- AC5 ✅ `merge-tree` clean. No conflicts with current `origin/user/yashasg/ecs_refactor`.

**Tests run:**
- Focused `[layout_cache]`: **64 assertions in 11 test cases — all pass**
- Full suite: **2671 assertions in 824 test cases — all pass**
- Build: zero compiler warnings (two pre-existing linker dup-lib warnings from vcpkg worktree setup, not introduced by this branch)

**Verdict:** ✅ APPROVED

## Learnings
- **Reusable quality note (#322):** When caching JSON layout data into POD structs, use `.at()` (throws on missing key) inside a `try/catch(nlohmann::json::exception)` block — this gives an explicit WARN log and returns `valid=false` without crashing. Optional sub-elements (like `lane_divider`) should be guarded by a separate `find_el` + optional `try/catch` so their absence doesn't invalidate the parent layout. This pattern matches the repo's "graceful degradation with flag" convention.
- **Reusable quality note (#322):** Integration tests against shipped JSON files (e.g. `gameplay.json`, `level_select.json`) are the correct second tier of layout cache tests — they catch field-name divergence between code and content authoring without needing a running game window.

### 2026-04-27 — Review: Archetype move to `app/archetypes/` (issue #344)

**Scope:** Reviewer gate on uncommitted archetype relocation: `app/systems/obstacle_archetypes.*` → `app/archetypes/obstacle_archetypes.*`.

**Verdict:** APPROVE WITH NON-BLOCKING NOTES

**Evidence:**
- Build clean, zero warnings (`-Wall -Wextra -Werror`). Only pre-existing ld duplicate-lib noise (unrelated).
- All 11 archetype tests pass (64 assertions, `[archetype]` tag).
- Switch statement exhaustive across all 8 `ObstacleKind` values — no missing case.
- Both callers (`beat_scheduler_system.cpp`, `obstacle_spawn_system.cpp`) updated to `#include "archetypes/obstacle_archetypes.h"`.
- Test file updated to `#include "archetypes/obstacle_archetypes.h"`.
- No stale includes to old `app/systems/obstacle_archetypes.*` path found anywhere.
- CMake: `ARCHETYPE_SOURCES` glob covers `app/archetypes/*.cpp`; added to `shapeshifter_lib` alongside `SYSTEM_SOURCES`. Include root `app/` is PUBLIC on `shapeshifter_lib`, so all consumers resolve the path correctly.
- Old files confirmed deleted from working tree.

**Non-blocking notes:**
1. `ObstacleArchetypeInput` field comment says "mask: LaneBlock, ComboGate" — LanePush kinds also use the struct (mask ignored). Doc-only gap.
2. Random spawner still selects `LaneBlock` as a kind index then immediately remaps to LanePush — vestigial mapping. Existing design, not introduced here.

**Relevant paths:** `app/archetypes/obstacle_archetypes.h`, `app/archetypes/obstacle_archetypes.cpp`, `app/systems/beat_scheduler_system.cpp`, `app/systems/obstacle_spawn_system.cpp`, `tests/test_obstacle_archetypes.cpp`, `CMakeLists.txt`

### 2026-04-27T23:30:53Z — P0 TestFlight: ecs_refactor Archetype Review (Issue #344)

**Status:** APPROVED WITH NON-BLOCKING NOTES

**Role in manifest:** Reviewer (background validation)

**Verdict:** Move is correct. Confirmed:
- Old path deleted
- Callers/tests updated
- No stale includes
- CMake `ARCHETYPE_SOURCES` wired into `shapeshifter_lib`
- Include roots resolve

**Non-blocking notes:**
- `ObstacleArchetypeInput.mask` comment omits LanePush behavior
- Random spawner has vestigial LaneBlock→LanePush mapping — deferred to issue #344


### 2026-05-XX — #SQUAD Comment Cleanup Review (shape_vertices.h, transform.h)

**Status:** APPROVED

**Files reviewed:** `app/components/shape_vertices.h`, `app/components/transform.h`, `tests/test_perspective.cpp`, `benchmarks/bench_perspective.cpp`, `app/systems/game_render_system.cpp`

**Verdict:** APPROVE. All removals are safe and correct.

**Confirmed:**
- `CIRCLE` table retained and actively used in `game_render_system.cpp` (rlgl 3D ring geometry, lines 107–117).
- `HEXAGON`, `SQUARE`, `TRIANGLE` tables had zero production references — confirmed by rg across app/, tests/, benchmarks/.
- Test deletions (HEXAGON/SQUARE/TRIANGLE test cases) exactly match deleted data. No orphaned tests.
- Benchmark deletion (hexagon iteration bench) matches deleted data.
- Zero `#SQUAD` markers remain.
- `Position`/`Velocity` retained as separate named structs is the **correct** ECS decision: distinct component types = distinct EnTT pools, no aliasing in registry views. Merging into `Vector2` would collapse two pools and invalidate view semantics across 21+ system files.
- Explanatory comment added to `transform.h` accurately documents the rationale.

**Non-blocking note (pre-existing, not introduced by this change):** `transform.h` includes `<cstdint>` but contains no `uint*` types. Orphaned include. Not a blocker; unrelated to this cleanup.

**Relevant paths:** `app/components/shape_vertices.h`, `app/components/transform.h`, `tests/test_perspective.cpp`, `benchmarks/bench_perspective.cpp`, `app/systems/game_render_system.cpp`

### 2026-04-27T23:58:23Z — #SQUAD Comment Cleanup Review (Final)

**Status:** APPROVED

**Wave:** Code cleanup task — review gate for #SQUAD comment fixes

**Review summary:** Confirmed all changes in `#SQUAD` comment cleanup are safe and architecturally correct.

**shape_vertices.h:** `HEXAGON`/`SQUARE`/`TRIANGLE` removal safe (zero production references). `CIRCLE` retention necessary for `game_render_system.cpp` rlgl 3D geometry path.

**transform.h:** `Position`/`Velocity` retention is correct ECS architecture — separate structs = separate EnTT component pools = no aliasing in views across 21+ system files. Merging to `Vector2` would collapse pools and break structural view semantics.

**Tests/benchmarks:** All deletions align exactly with deleted data. Circle and floor ring coverage preserved. No orphaned tests.

**Non-blocking note (pre-existing):** `transform.h` includes `<cstdint>` with no `uint*` types (orphaned). Not introduced by this cleanup. Low priority.

**Decision records:** Approved and merged to decisions.md.

**Orchestration log:** `.squad/orchestration-log/2026-04-27T23:58:23Z-kujan.md`

### 2026-04-28 — Review: ECS Cleanup Batch #273/#333/#286/#336/#342/#340

**Scope:** Keaton (#273/#333), Hockney (#286), Baer (#336/#342), McManus (#340)

**Test baseline confirmed:** 2808 assertions / 840 test cases — all pass. Zero warnings.

**Verdict per issue:**
- **#273 (ButtonPressEvent semantic payload):** ✅ APPROVED. Entity handle fully replaced with value payload. Encoding at hit-test time. Stale-entity safety test present. Non-blocking: `player_input_system.cpp` section heading "EventQueue consumption contract" still refers to deleted struct by name.
- **#333 (InputEvent dispatcher Tier-1 + EventQueue deletion):** ✅ APPROVED. EventQueue deleted. R7 guard in place. Two-tier architecture documented. Tier-1 sinks registered, unwired, and tested. Non-blocking: R2 (no per-frame cap) — no regression observed; low risk.
- **#286 (SettingsState/HighScoreState helpers):** ✅ APPROVED. Both structs plain data; helpers in persistence namespaces. All call sites updated. 170 assertions pass.
- **#336 (miss_detection exclude-view regression):** ✅ APPROVED. 28 assertions / 5 test cases, live-verified. No PLATFORM guard.
- **#342 (signal lifecycle ungated):** ✅ APPROVED. 16 assertions / 6 test cases, live-verified. No PLATFORM guard. Doc discrepancy: inbox claimed 23 assertions, file has 16 (non-blocking).
- **#340 (SongState/DifficultyConfig ownership comments):** ✅ APPROVED. Comments-only. All system attributions verified against actual `app/systems/` files.

**All six issues are closure-ready.**

**Reusable quality notes:**
- When deleting a hand-rolled queue in favour of `entt::dispatcher`, update ALL doc comments referencing the deleted struct name — including section headings in consumer systems and component-level descriptions.
- Signal lifecycle tests (on_construct/on_destroy) should always be non-platform-gated; they test ECS wiring, not platform capabilities.
- Doc assertion counts in inbox files should match the file's actual CHECK/REQUIRE count — check before filing.

---

### 2026-05-18 — Review: UI ECS Batch (#337, #338, #339, #343)

**Scope:** Keyser-authored UI ECS batch. Code changes landed in `fdefeb1`; decision documentation in `ec891a9`.

**Verification performed:**

**#337 — UIActiveCache startup initialization:**
- `app/game_loop.cpp:118`: `reg.ctx().emplace<UIActiveCache>();` confirmed present.
- `tests/test_helpers.h:53`: `reg.ctx().emplace<UIActiveCache>();` confirmed present.
- `active_tag_system.cpp`: uses `reg.ctx().get<UIActiveCache>()` (hard crash if absent) — lazy `find/emplace` pattern removed.
- Diff confirmed in `fdefeb1`: exactly the lazy-init lines removed and replaced with bare `.get<>()` calls.
- Existing `[hit_test][active_tag]` tests cover both `invalidate_active_tag_cache` and `ensure_active_tags_synced` via `make_registry()`. All pass.

**#338 — Safe JSON access in spawn_ui_elements:**
- Diff confirmed in `fdefeb1`: all `operator[]` accesses on required fields replaced with `.at()`.
- Three error classes handled correctly:
  - Animation block (optional): `try/catch` → `[WARN]` stderr, entity preserved. ✅
  - Button required colors (`bg_color`, `border_color`, `text_color`): outer `try/catch` → `[WARN]` + `reg.destroy(e)` + `continue`. ✅
  - Shape required color: `try/catch` → `[WARN]` + `reg.destroy(e)` + `continue`. ✅
- **Non-blocking gap:** Zero tests exercise the error paths. All three `catch` blocks are untested. Future regression could silently delete them. Recommend a hardening ticket with tests for malformed-JSON spawn (missing `bg_color`, missing animation sub-key, missing shape `color`).

**#339 — UIState.current hashing:**
- `UIState.current` is `std::string` in `app/components/ui_state.h`. No code change needed or made.
- Rationale is sound: comparison at screen-load boundaries only, SSO handles short names, element-level hot-path already uses `entt::hashed_string` (#312).
- Decision documented in `keyser-ui-ecs-batch.md`. ✅

**#343 — Dynamic text allocation:**
- `resolve_ui_dynamic_text` called per-frame per entity in render system. ~2–5 entities, strings < 20 chars.
- SSO threshold (libc++ ~15–22 chars) means most are stack-local. 300 ops/sec max.
- Cache invalidation complexity (compare-each-frame or event wiring) > zero measurable benefit.
- Decision documented in `keyser-ui-ecs-batch.md`. ✅

**Test run:** `./run.sh test` — **2808 assertions / 840 test cases** — all pass. Independently verified.

**Verdict: ✅ APPROVED**

All four issues are closure-ready. One non-blocking hardening gap noted for #338 (error-path test coverage).

**Hardening ticket recommendation:** Add tests for `spawn_ui_elements` malformed JSON paths (animation missing sub-key, button missing `bg_color`, shape missing `color`) to ensure the try/catch error paths are regression-protected.

**Reusable quality note:**
- When adding defensive `try/catch` error paths in a spawn function, add at minimum one test per error class to lock in the behavior. Absence of error-path tests means future refactors can silently delete the catch blocks.

### 2026-04-27 — Review: #335 and #341 (Keaton, commit a9ed3fc)

**Scope:** TestPlayerState planned entity lifecycle contract documentation + SKILL.md ctx guidance correction.

**Findings:**
- #335: `LIFETIME CONTRACT` comment placed correctly above `planned[]`. Two new test cases verify stale-pruning and live-entry retention. `test_player_clean_planned()` confirmed as first substantive call in system tick (after frame counter increment, before all entity logic). Invariants match implementation.
- #341: SKILL.md table correctly split into two rows distinguishing `ctx().emplace<T>()` (startup one-time, absent-type required) from `ctx().insert_or_assign()` (upsert boundary). Misleading `NOT emplace_or_replace` comment removed. No gameplay code touched.
- Full suite: 2818 assertions / 842 test cases — all pass.

**Verdict:** ✅ APPROVED. Both issues closure-ready. No regressions.

### 2026-04-27 — Review: #346 (Baer, commits 710ff34 + ff96be8)

**Scope:** Expose `spawn_ui_elements()` from `ui_navigation_system.cpp` into `ui_loader.cpp/h`; add 12 regression tests for malformed-JSON catch branches.

**Background:** In the #338 batch review I flagged that all three catch branches inside `spawn_ui_elements()` lacked test coverage and recommended a hardening ticket. #346 is the direct resolution.

**Findings:**

- **Function relocation:** `spawn_ui_elements()` and its four helpers (`skip_for_platform`, `json_font`, `json_align`, `json_shape_kind`) faithfully moved from `ui_navigation_system.cpp` to `ui_loader.cpp`. Zero behavior change confirmed by diffing the two copies — identical logic.
- **`json_color_rl` null-safety patch:** Added `if (!arr.is_array() || arr.size() < 3) return WHITE;` guard. Correct and safe; the old version would dereference an unvalidated index.
- **Public API declaration:** Declaration in `ui_loader.h` includes a comment explaining the test-exposure rationale. Controlled and correct.
- **ui_navigation_system.cpp:** Removed all now-dead statics; `spawn_ui_elements(reg, ui.screen)` call unchanged. Clean.
- **Tests (12 test cases, 27 assertions):** All four catch branches exercised:
  - Text animation catch (missing `speed` key, wrong `alpha_range` type)
  - Button outer catch (missing `bg_color`, missing `border_color`)
  - Button inner animation catch (bad animation survives with `UIButton`)
  - Shape outer catch (missing `color` field)
  - Positive paths: valid text+animation, valid shape, mixed valid+invalid elements, empty/missing elements array.
  - All tests use in-memory JSON — deterministic, no on-disk fixtures.
- **Orphan entity for composite types (non-blocking observation):** `spawn_ui_elements()` does not handle `stat_table`, `card_list`, `line`, `burnout_meter`, etc. For these types, entities are created with `UIElementTag` + `Position` but no visual component. Pre-existing behavior, not introduced by this PR. The orphans are benign: multi-component render queries skip them; `destroy_ui_elements()` cleans them on screen transition. Composite types are served by the layout cache system, not the entity spawn path. Warrants a separate follow-on issue for explicit `continue` + debug-log on unknown type.
- **Build:** Zero compiler warnings (`-Wall -Wextra -Werror`). Clean.
- **Full suite:** 854 test cases, 2845 assertions — all pass. (Author reported 837/2845 — minor discrepancy likely from platform/environment; assertions match exactly.)

**Verdict: ✅ APPROVED — closure-ready.**

Non-blocking note: open a follow-on issue for `spawn_ui_elements` to `continue` + warn on unrecognized element types, to prevent silent orphan accumulation if new composite types are added to `kSupportedTypes` without a spawn branch.

---

### 2026-05-18 — Review: Model Authority Slice (Keaton/Baer — Slice 1 + Slice 2)

**Scope:** Keaton implementation of ObstacleScrollZ bridge-state migration for LowBar/HighBar + ObstacleModel GPU RAII + Baer test coverage. Reviewed against: `kujan-model-slice-criteria.md` (B1–B9), `keyser-model-authority-audit.md` (HB-1–5), and user's stated requirements.

**Verdict:** ❌ REJECTED — 4 blocking findings. Keaton locked out. Revision owner: McManus (primary) + Keyser (co-owner on BF-3/BF-4).

**Blocking findings:**

**BF-1 (HIGH) — `LoadModelFromMesh` used in `shape_obstacle.cpp:117`**
- `Mesh mesh = GenMeshCube(...); Model model = LoadModelFromMesh(mesh);`
- User requirement and criteria B2 explicitly prohibit this call in the obstacle path.
- Required: manual `RL_MALLOC` + mesh array construction, material assignment, `meshMaterial` index init.

**BF-2 (HIGH) — Render path does not use owned ObstacleModel; camera writes legacy ModelTransform**
- `camera_system.cpp:263–274` emits `ModelTransform` (via `get_or_emplace`) for LowBar/HighBar from `ObstacleScrollZ`.
- `game_render_system.cpp` has no `<ObstacleModel, TagWorldPass>` draw loop; `draw_meshes()` reads only `ModelTransform` + `ShapeMeshes`.
- The owned mesh allocated by `build_obstacle_model()` is never drawn. `TagWorldPass` emplacement in `build_obstacle_model()` is dead code.
- GPU resource is allocated, RAII-guarded, and destroyed — but never rendered. Violates criteria B5 and B7.

**BF-3 (HIGH) — `app/systems/obstacle_archetypes.*` still present (ODR violation)**
- CMakeLists.txt uses `file(GLOB SYSTEM_SOURCES app/systems/*.cpp)` AND `file(GLOB ARCHETYPE_SOURCES app/archetypes/*.cpp)`.
- Both `app/systems/obstacle_archetypes.cpp` AND `app/archetypes/obstacle_archetypes.cpp` define `apply_obstacle_archetype`. Linker ODR violation.
- These were intentionally deleted in a prior cleanup. `test_obstacle_archetypes.cpp` still includes `"systems/obstacle_archetypes.h"` — not the canonical `"archetypes/obstacle_archetypes.h"`.

**BF-4 (MEDIUM) — `ObstacleParts` is an empty tag, not a geometry descriptor**
- `rendering.h:105`: `struct ObstacleParts {};` — no geometry fields.
- Criteria B4 requires typed descriptors (x, w, h, d per slab) alongside the Model so `scroll_system`/camera can recompute `model.transform` per frame without reading raw vertex buffers.
- Without these, the correct render path (BF-2 fix) cannot reconstruct per-part origin matrices at scroll time.

**What passes:**
- LowBar/HighBar archetype correctly emits `ObstacleScrollZ`, not `Position` ✅
- All lifecycle systems (scroll, cleanup, miss, collision, scoring) correctly implement the two-view bridge-state pattern ✅
- `IsWindowReady()` headless guard in `build_obstacle_model()` correct ✅
- `on_obstacle_model_destroy` owned-flag double-unload protection correct ✅
- `ObstacleModel` RAII move semantics correct ✅
- All 898 test assertions pass ✅

**Test coverage note:**
Tests correctly cover bridge-state contract for scroll, cleanup, miss, collision, scoring. They do NOT catch BF-1/BF-2 because the render path for owned models is never tested — tests only verify structural component presence/absence and score/miss/cleanup logic. Tests do not and cannot detect that the allocated GPU mesh is never drawn.

**Learning:** "LoadModelFromMesh prohibition" and "owned-model render path completeness" are the two hardest gaps to detect from tests alone. Future criteria must include an explicit render-path integration test: a spawned LowBar with a mock GPU context must produce a `DrawMesh` call from `ObstacleModel.model`, not from `ShapeMeshes`. Without this test shape, the render path gap propagates through review.

### 2026-05-18 — Re-Review: Model Authority Revision (McManus — BF-1..BF-4)

**Scope:** McManus revision resolving the four Kujan blockers from the Keaton/Baer rejection.

**Verdict:** ❌ REJECTED — 1 new blocking defect (RF-1) + 1 test gap (RF-2). McManus locked out. Revision owner: Keyser.

**What was fixed correctly:**
- **BF-1 ✅** — `LoadModelFromMesh` removed; manual `RL_MALLOC` construction correct; `UploadMesh` correctly omitted for raylib 5.5 (GenMesh* already uploads).
- **BF-3 ✅** — `app/systems/obstacle_archetypes.*` deleted; stale includes gone; ODR resolved.
- **BF-4 ✅** — `ObstacleParts` populated with 6 POD float fields; `static_assert(!std::is_empty_v<ObstacleParts>)` added.
- **BF-2 render path ✅** — `draw_owned_models()` added in `game_render_system`; camera section 1b writes `om.model.transform`; no `ModelTransform` emitted for LowBar/HighBar. Draw paths are mutually exclusive.

**Blocking finding RF-1 — Double-scale on owned mesh:**
`build_obstacle_model()` creates `GenMeshCube(SCREEN_W_F, height, dsz->h)` — a pre-dimensioned mesh with vertices already spanning ±360 in X. The camera system then calls `slab_matrix(..., pd.width=720, pd.height=30, pd.depth=40)` which applies `MatrixScale(720, 30, 40)` to those already-dimensioned vertices. The shared slab mesh (`sm.slab = GenMeshCube(1.0f, 1.0f, 1.0f)`) shows the correct pattern: slab_matrix assumes a unit mesh. Fix: `GenMeshCube(1.0f, 1.0f, 1.0f)` in `build_obstacle_model()`. ObstacleParts width/height/depth values remain correct (they feed slab_matrix as intended dimensions).

**Test gap RF-2 — BF-2 regression test doesn't validate scale components:**
The test at `test_obstacle_model_slice.cpp:424–449` only checks that translation components are non-zero. It cannot detect double-scaling because any non-zero scaling still produces non-zero translations. Keyser must add a test that verifies diagonal scale entries (m0, m5, m10 for the final matrix) equal expected width/height/depth.

**Learning:** Any time a mesh is pre-dimensioned (GenMeshCube with actual dimensions), a transform-only (translate-only) matrix must be used. Any time slab_matrix or another scale-compose function is used, the input mesh must be a unit mesh. Tests that verify transform "non-identity" are insufficient — they must verify the actual scale components against expected values to catch double-scaling.

### 2026-04-28 — Gate Addendum: render_tags.h surface (cleanup slice, Keyser revision)

**Scope:** Model authority cleanup slice — `app/components/render_tags.h` added as untracked file in violation of directive `copilot-directive-20260428T073749Z-no-render-tags-component.md`.

**Verdict:** ❌ BLOCKING gate addendum written. Revision owner: Keyser.

**Finding:**
- `render_tags.h` is a net-new untracked component header containing `TagWorldPass`, `TagEffectsPass`, `TagHUDPass`.
- Only `TagWorldPass` has production use (`draw_owned_models` view + `shape_obstacle.cpp` emplace). `TagEffectsPass` and `TagHUDPass` are test-only.
- Directive explicitly prohibits new component headers during this cleanup pass.

**Required action:**
- Fold all three tag structs into the existing `app/components/rendering.h`.
- Delete `render_tags.h`.
- Update all include paths (production and test).
- No new header files are acceptable.

**Learning:** Cleanup slices are especially prone to surface accumulation: a single `TagWorldPass` needed for a render path view silently attracts a new component header. The cleanup directive must be checked at diff boundary, not just at feature boundary. A new `app/components/*.h` file in a "cleanup" diff is always a red flag requiring directive review before accepting.

**Gate file:** `.squad/decisions/inbox/kujan-render-tags-gate.md`

### 2026-04-28 — Component Cleanup Pass Review

**Scope:** Full component cleanup batch — deletion of non-component headers, render_tags fold, ObstacleScrollZ migration (LowBar/HighBar), new entities layer.

**Gate outcome:** ❌ REJECTED — build broken at time of inspection.

**What was correct:**
- `render_tags.h` properly folded into `rendering.h`; standalone deleted; includes cleaned up in production code
- 7 non-component/wrong-location headers deleted from `app/components/`: `audio.h`, `music.h`, `obstacle_counter.h`, `obstacle_data.h`, `ring_zone.h`, `settings.h`, `shape_vertices.h`
- `ObstacleScrollZ`, `ObstacleModel`, `ObstacleParts` are proper entity-data components added to existing headers (not new headers)
- `TagWorldPass/EffectsPass/HUDPass` folded into `rendering.h` correctly
- `app/systems/obstacle_archetypes.*` dedup removed; replaced by `app/entities/obstacle_entity.*`

**Build errors found:**
1. `app/entities/obstacle_entity.cpp` includes deleted `obstacle_data.h`
2. `test_obstacle_archetypes.cpp` has 13 `beat_info` field initializer errors (old API)
3. `test_obstacle_archetypes.cpp` includes deleted `archetypes/obstacle_archetypes.h`

**Reviewer lockout:** Keaton locked out. Keyser owns the revision fix.

**Still-deferred:** `ui_layout_cache.h` (context singleton in components/) — not part of this slice.

**Learnings:**
- Implementation agents modifying the working tree during review cause state inconsistency; always re-run `git diff HEAD` and the build at gate time, not just at inspection start
- The "two-view migration" pattern (Position + ObstacleScrollZ dual views) is established as canonical for zero-regression ECS component migrations in this project
- When deleting a component header, ALL transitive includes must be audited — not just the direct consumers in `app/components/` callers, but new files added in the same batch (like `entities/obstacle_entity.cpp`)

### 2026-04-29 — Re-Review: Component Cleanup / Entity Layer / Model Scope

**Scope:** Keyser-fixed revision of Keaton's cleanup batch (commit `0d642e2`).

**Prior rejection blockers all resolved:**
- `obstacle_entity.cpp` no longer includes deleted `obstacle_data.h`.
- `test_obstacle_archetypes.cpp` uses `spawn_obstacle` + `ObstacleSpawnParams` — no missing-field-initializer warnings.
- `test_obstacle_archetypes.cpp` includes `entities/obstacle_entity.h` (not deleted `archetypes/obstacle_archetypes.h`).

**All gate criteria pass:**
- All 10 deleted headers have zero includes remaining in `app/` and `tests/`.
- `entities/obstacle_entity.*` compiles cleanly, wired via `file(GLOB ENTITY_SOURCES)` in CMakeLists.
- `archetypes/obstacle_archetypes.*` and `systems/obstacle_archetypes.*` are both gone; no duplicate bundle API.
- `render_tags.h` gone as standalone; tags folded into `rendering.h`; Slice 0 tests enabled correctly.
- `Model.transform` scope narrow — `ObstacleScrollZ` emitted only for `LowBar`/`HighBar` in `obstacle_entity.cpp`.
- Build: 2983 assertions / 887 test cases pass, zero warnings, diff-check clean (verified locally).

**Non-blocking notes:**
- `app/components/difficulty.h.tmp` leftover; not included anywhere; housekeeping ticket warranted.
- `ui_layout_cache.h` remains as acknowledged deferred item (out of this slice's scope).

**Verdict:** APPROVED

---

### 2026-04-28 — Final Session Completion: ECS Cleanup Approval Scribe Log

**Completion status:** APPROVED ✅ — All team deliverables logged and committed.

Scribe tasks completed:
- Orchestration logs written for all 7 agents (Hockney, Fenster, Baer, Kobayashi, McManus, Keyser, Kujan)
- Session log written: ECS cleanup approval batch
- Decision inbox merged into decisions.md; 6 merged decisions; inbox files deleted
- Agent history files updated with team context
- Git commit staged and ready

The cleanup wave is complete. Next phase: await merge.

---

### 2026-05-18 — Systems Inventory (read-only audit)

**Scope:** Full classification of all 44 files in `app/systems/`. Requested by yashasg: "systems that aren't actually systems" + raylib directive for input/gestures.

**Key findings:**

**Correctly placed (19 files):** beat_scheduler, collision, energy, lifetime, miss_detection, obstacle_despawn, particle, player_movement, scoring, scroll, shape_window, song_playback, popup_display, game_state, game_render, ui_render, camera_system, audio_system, haptic_system.

**Event listeners misnamed as systems (5 files to consolidate/rename):**
- `gesture_routing_system.cpp` — 14 lines, one sink callback. Fold into input_dispatcher.cpp.
- `input_dispatcher.cpp` — pure wiring. Rename to input_wiring or fold.
- `level_select_system.cpp` — click handlers + suspicious duplicate disp.update<> drain. Rename to level_select_handler.cpp.
- `player_input_system.cpp` — two event callbacks, no frame loop. Rename to player_input_handler.cpp.
- `active_tag_system.cpp` — cache sync utility, called from hit_test not main loop. Rename.

**Loaders/util in wrong location (6 files to move to app/util/):**
- `play_session.cpp/.h` — one-shot session initializer
- `ui_loader.cpp/.h` — JSON file parser
- `ui_source_resolver.cpp/.h` — string→component data accessor
- `session_logger.cpp/.h` — file I/O logger (beat_log_system stays linked)
- `sfx_bank.cpp` — procedural DSP + audio service init
- `text_renderer.cpp/.h` — font load + text draw (CMake already excludes from lib)

**Data types in wrong location (2 headers to move to app/components/):**
- `audio_types.h` → `app/components/audio.h`
- `music_context.h` → fold into same

**Entity factories in wrong location (2 to move to app/entities/):**
- `ui_button_spawner.h`
- `obstacle_counter_system.*`

**Raylib directive violation (1 file — HIGH priority):**
- `input_gesture.h` — custom swipe/tap classifier conflicts with user directive to use raylib gesture helpers (`IsGestureDetected`, `GetGestureDragVector`). Zone-split logic must be preserved in input_system.cpp. Test migration required.

**CMake note:** `file(GLOB SYSTEM_SOURCES app/systems/*.cpp)` means moving files automatically removes them from the glob. UTIL_SOURCES, ENTITY_SOURCES globs cover the target directories. Explicit CMake exclusion rules for `text_renderer.cpp`, `camera_system.cpp` etc. must be updated when those files move.

**Recommended wave order:** A (header moves) → B (renames/consolidations) → C (small util moves) → D (heavier util moves) → E (raylib input replacement + level_select drain audit).

**Output:** `.squad/decisions/inbox/kujan-systems-inventory.md`

### 2026-05-18 — System Boundary Cleanup Review (Keaton)

**Scope:** `beat_map_loader` move to `app/util/` + `cleanup_system` → `obstacle_despawn_system` rename + Z-despawn camera fix.

**Verdict:** ❌ REJECTED — 2 blocking defects. Keaton locked out. Revision owner: Keyser.

**What was correct:**
- `app/util/beat_map_loader.h/.cpp` present with unchanged content; `app/systems/` copies deleted.
- All 4 Keyser-listed include sites updated to `"util/beat_map_loader.h"`. No stale includes remain.
- `all_systems.h`, `game_loop.cpp`, `test_world_systems.cpp` all correctly renamed to `obstacle_despawn_system`.
- `app/components/obstacle.h` comment updated.

**BL-1 — `benchmarks/bench_systems.cpp` (lines 192, 196, 215, 235): 4 stale `cleanup_system` calls.**
Rename not applied to benchmarks. Build-breaking "use of undeclared identifier" on compile.

**BL-2 — `obstacle_despawn_system.cpp` line 20: `ObstacleScrollZ` path still uses `constants::DESTROY_Y` (1400.0f) instead of `GameCamera.cam.position.z` (1900.0f).**
The directive acceptance criterion is explicit: query `reg.view<GameCamera>()`, use `cam.cam.position.z`, fall back to `DESTROY_Y` when no camera exists (headless tests). The function comment claims "camera's far-Z boundary" but the constant makes that a lie.

**Gate file:** `.squad/decisions/inbox/kujan-boundary-cleanup-review.md`

**Learning:** Rename propagation must cover benchmarks/ as well as tests/ — these are separate directories that both call named system functions. Any "rename a system function" task checklist must include `grep -r old_name benchmarks/` explicitly. Also: a comment claiming "camera-derived" threshold while the code uses a constant is a review signal that the key requirement was deferred or forgotten.

### 2026-05-XX — Review: Screen Controller Migration (adapter removal, "start fresh")

**Scope:** Remove `app/ui/adapters/` layer entirely; replace with `app/ui/screen_controllers/`. Input artifacts: CMakeLists.txt, `app/systems/ui_render_system.cpp`, `app/ui/screen_controllers/*`, `app/ui/generated/*_layout.h`, `app/ui/raygui_impl.cpp`, `design-docs/rguilayout-portable-c-integration.md`.

**Build:** Zero warnings, 2635 assertions / 901 test cases — all pass ✅

**AC findings:**

- **AC1 — No adapter layer remains:** ✅ `app/ui/adapters/` deleted (working tree). CMakeLists swapped `UI_ADAPTER_SOURCES` → `UI_SCREEN_CONTROLLER_SOURCES`. Zero adapter `#include` paths remain in `app/`.
- **AC2 — Dispatch semantics preserved:** ✅ All 8 controllers replicate adapter dispatch exactly. `GameState` transitions, `InputState.quit_requested`, `dispatch_end_screen_choice`, `PausedController` MenuButtonTag/phase reset — all preserved 1:1.
- **AC3 — No ECS layout mirror introduced:** ✅ `HudLayout`/`LevelSelectLayout` are pre-existing (#322). Screen controllers hold `LayoutState` as direct template member; nothing pushed into `reg.ctx()`.
- **AC4 — Helper extraction is not slop:** ✅ `RGuiScreenController<>` template reduces ~30 lines/screen (8 screens). `dispatch_end_screen_choice<>` collapses a repeating 3-branch pattern. `offset_rect()` is a 1-liner. No virtual dispatch; compile-time only.
- **AC5 — No dead code or stale includes:** ⚠️ Minor. Three generated headers (`gameplay_hud_layout.h`, `settings_layout.h`, `level_select_screen_layout.h`) retain stale "This adapter provides…" in comments. `init_*_screen_ui()` functions are dead exports — base class lazily inits on first render; the explicit init functions are never called anywhere. Not harmful; non-blocking.
- **AC6 — Spec not misleading:** ❌ **BLOCKING.** `design-docs/rguilayout-portable-c-integration.md` contains an entire "Migration Path" section (lines 305–343) and a "Legacy note" (line 271) that describe adapters as the *current* state and screen controllers as the *target* state. With adapters deleted, this is now actively backwards. Also: checklist item (line 422) shows migration as pending; code examples use `reg.ctx<Dispatcher>()` old EnTT API and fictional `GameCommand`/`Dispatcher` types that don't exist in the codebase.

**Verdict:** ❌ **REJECTED on AC6.** Keaton is locked out per rejection protocol. Assigned to **Fenster** (doc specialist, has prior spec edits on this file, not locked out).

**Reusable quality note:** When a migration removes a legacy layer, the spec docs must be updated atomically with the code change — specifically: delete or archive the "migration path" section, remove "legacy note" caveats, and update any code examples to use live API. Leaving the migration path in place after completion creates an inversion trap: new contributors will implement the deleted layer thinking it's the current pattern.

### 2026-05-XX — Re-Review: Screen Controller Migration Spec (Fenster revision)

**Scope:** Re-review of `design-docs/rguilayout-portable-c-integration.md` after Fenster revision addressing AC6 blocker from prior rejection.

**Prior blockers verified fixed:**
- `GameCommand`/`Dispatcher`: **zero hits** in spec, screen controllers, and generated headers. ✅
- Migration section (`"Migration Path"` describing adapters as current): **replaced** with `## Migration History: Adapter Naming → Screen Controllers (Completed)` — explicitly marked complete, past tense, no current/target framing. ✅
- Checklist migration item was pending: **now checked** (`[x] Legacy app/ui/adapters/` migrated). ✅
- Code examples: both the "incorrect" and "correct" pattern blocks now use actual `RGuiScreenController<TitleLayoutState, &TitleLayout_Init, &TitleLayout_Render>` and `reg.ctx().get<GameState>()` — matching live code exactly. ✅
- Generated-comment adapter wording: **zero adapter references** in generated headers (`app/ui/generated/`). ✅

**Non-blocking notes (not blockers):**
- `raygui_impl.cpp` entry in the file layout section (§ File Layout and Naming) retains comment `# Single RAYGUI_IMPLEMENTATION definition (future build integration)` — the file exists and is already wired in CMakeLists.txt (line 408). Minor stale label, no guidance impact.
- `### CMake Integration (Future)` section heading — CMake is already integrated. Heading mislabels completed work; example commands remain valid guidance for anyone extending the build.
- The "Render System Call" illustrative snippet uses `reg.ctx<GamePhase>()` (old EnTT API); live render system uses `reg.ctx().get<>()`. The normative "Correct Pattern" section shows the right form, so this is a minor inconsistency in a non-normative illustrative block.

**Verdict:** ✅ **APPROVED.** All three AC6 blockers are resolved. Remaining items are non-blocking doc stalenesses.

**Reusable quality note:** When re-reviewing a doc revision, start from the explicit prior blockers and verify each is fully resolved before scanning for new issues. The threshold for approving is: all prior material blockers fixed AND no new material architectural misguidance introduced. Doc stalenesses in illustrative/non-normative blocks do not rise to rejection level when the normative patterns are correct.

### 2026-05-XX — Title Screen Layout + Controller Review (Hockney revision)

**Scope:** Title screen text readability, "SET" button relocation, runtime override removal.
**Verdict:** ✅ **APPROVED WITH NOTES**

**What was verified:**

| Criterion | Result |
|---|---|
| Runtime override (manual GuiLabel/GuiButton) removed | ✅ Zero hits in controller |
| Controller calls `title_controller.render()` | ✅ |
| `.rgl` geometry matches Redfoot spec | ✅ TitleText (40 200 640 96), StartPrompt (40 640 640 56), ExitButton (260 1080 200 56), SettingsButton (632 1170 64 64) |
| Generated header matches `.rgl` | ✅ |
| "SET" removed; gear icon `#142#` (ICON_GEAR_BIG) at bottom-right 64×64 | ✅ |
| Settings routes to `GamePhase::Settings` | ✅ |
| ExitButton gated with `#ifndef PLATFORM_WEB` | ✅ |
| Build: zero warnings | ✅ |
| Tests: all pass (2595 assertions) | ✅ |

**Note (non-blocking):** `GuiSetStyle(DEFAULT, TEXT_SIZE, 28)` applies to all elements uniformly. Redfoot's spec wants 64px for SHAPESHIFTER, 28px for TAP TO START, but per-element font sizing within a single generated `TitleLayout_Render` call is not achievable without reintroducing interleaved manual style changes (the very pattern we just removed). Current uniform 28px is architecturally correct. If visual feedback shows the title is insufficiently prominent, the fix is injecting per-element style hooks into `RGuiScreenController<>` template infrastructure — separate scope.

**Reusable criteria:**
- Generated `TitleLayout_Render` is a single-pass draw: any per-element style differentiation requires either interleaved calls (override smell) or template infrastructure changes. Uniform style is the clean tradeoff.
- Icon ID verification matters: `#142#` = `ICON_GEAR_BIG` (not GEAR=141). Document which icon ID maps to which visual in review notes.
- `#ifndef PLATFORM_WEB` guard for exit-button handling is the correct pattern for desktop-only controls.

### 2026-04-29 — Title Screen Review (approved with notes)

**Session:** Title Screen UI Fix  
**Artifact reviewed:** Hockney's title screen changes  
**Verdict:** ✅ APPROVED WITH NOTES

Verified all blocking acceptance criteria from Redfoot: runtime override removed, generated layout active as source of truth, "SET" replaced with bottom-right 64×64 gear (#142#), settings wired to phase transition. Build: zero warnings. Tests: 2595 assertions pass.

Non-blocking note: `GuiSetStyle(DEFAULT, TEXT_SIZE, 28)` uniform across all labels. Per-element sizing would require re-introducing interleaved manual draws (override pattern smell). Uniform 28px is architecturally correct; per-element styling infrastructure is deferred scope.

**Reusable criteria:** Generated single-pass render functions must have uniform styles unless infrastructure supports per-element injection.

### 2026-05-XX — Review: Settings gear click fix (raygui letterbox hit-mapping)

**Scope:** Hockney + Baer — `SetMouseOffset/Scale` bracketing in `ui_render_system.cpp`; `title_screen_controller.cpp` Settings routing; regression tests in `test_game_state_extended.cpp` and `test_world_systems.cpp`.

**User issue:** "i see the settings button but it doesnt do anything?" — confirmed root cause: raygui hit-testing in window-space while UI rendered in 720×1280 virtual space under letterboxing.

**Mouse transform math (ui_render_system.cpp lines 69–74):**
- `SetMouseOffset(lround(-offset_x), lround(-offset_y))` + `SetMouseScale(1/scale, 1/scale)` — correct formula. raygui internal transform: `adjusted = (raw - offset) * invScale`, so `adjusted = (window - offset_x) / scale = virtual`. ✅
- `std::lround` → `static_cast<int>` for the integer `SetMouseOffset` API — correct rounding. ✅
- `scale > 0.0f` guard prevents divide-by-zero. ✅
- Uniform `inv_scale` correct: `ScreenTransform` uses a single `scale` field. ✅

**Restore safety:**
- `input_system` runs BEFORE `ui_render_system`. Uses own `to_vx`/`to_vy` lambdas — does NOT depend on `SetMouseOffset/Scale` global state. ✅
- Restore `(0,0)/(1,1)` unconditional even when `scale ≤ 0` path skipped transform — idempotent, correct. ✅

**Settings routing:**
- `SettingsButtonPressed` → `gs.transition_pending = true; gs.next_phase = GamePhase::Settings` — direct, no adapters, no JSON/ECS UI render loops. ✅
- Button rect `{632, 1170, 64, 64}` (bottom-right `#142#` gear icon). ✅

**Tests:**
- `test_world_systems.cpp` Settings phase unit test — narrow, correct.
- `test_game_state_extended.cpp` Settings navigation proxy — broader, `SKIP` guard correct and documented.
- Acknowledged GuiButton seam gap in Baer's decision memo — acceptable.

**Verdict:** ✅ APPROVED

**Reusable quality note (raygui letterbox):** Bracket ALL raygui calls in `ui_render_system` with `SetMouseOffset(-offset_x, -offset_y)` / `SetMouseScale(1/scale, 1/scale)` and unconditional restore. Apply at the system level, not per screen-controller. `scale > 0` guard is mandatory.

### Review: standalone UI cleanup (2026)
**Verdict:** APPROVED  
**Criteria met:**
- `app/ui/generated/standalone/` confirmed deleted (all 17 files show `D` in git status); zero references in CMakeLists.txt or any `.cpp`/`.h` source file — deletion is safe.
- All 8 `*_layout.h` embeddable headers and all `.rgl` authoring files intact in their active paths.
- `generate_embeddable.sh`, `INTEGRATION.md`, `SUMMARY.md`, and `design-docs/rguilayout-portable-c-integration.md` all consistently state standalone exports are scratch-only under `build/rguilayout-scratch/` (covered by `.gitignore`'s `build/` rule) and must not be committed.
- Hockney decision inbox present and consistent with implementation.
- No unrelated dirty-file churn introduced by this change; other modifications in working tree predate this task.

### 2026-04-29 — Review: remove vendored raygui, add via vcpkg (#user-directive)

**Scope:** Hockney implementation — remove `app/ui/vendor/raygui.h`, switch `raygui_impl.cpp` to `#include <raygui.h>`, add `raygui` to `vcpkg.json`, wire `find_path(RAYGUI_INCLUDE_DIR raygui.h REQUIRED)` as SYSTEM include on `shapeshifter_lib`.

**Evidence confirmed:**
- `app/ui/vendor/` directory: absent. Zero `vendor/raygui` references in source or CMake.
- `vcpkg.json`: contains `"raygui"` in dependencies. ✅
- `CMakeLists.txt`: `find_path(RAYGUI_INCLUDE_DIR raygui.h REQUIRED)` → `target_include_directories(shapeshifter_lib SYSTEM PUBLIC ${RAYGUI_INCLUDE_DIR})`. ✅
- `app/ui/raygui_impl.cpp`: exactly one `#define RAYGUI_IMPLEMENTATION`, includes `<raygui.h>`, wraps third-party code in compiler warning suppression pragmas (clang/gcc/MSVC). Exactly one definition site in the whole project. ✅
- All screen controllers use `#include <raygui.h>` (angle brackets, system include path). ✅
- `ui_render_system.cpp`: no JSON access, no UIElement view loops, no adapter wiring. Exclusively calls screen controller render functions. ✅
- `app/ui/generated/*_layout.h` headers: none define `RAYGUI_IMPLEMENTATION`. ✅

**Non-blocking findings (doc staleness):**
1. `tools/rguilayout/SUMMARY.md` §2 (line 76-78), §3 (line 81-88), and §CONCLUSION "Next steps" (line 237) still say raygui is not in the build and RAYGUI_IMPLEMENTATION is undefined — both now FALSE. Materially misleading to anyone consulting this doc. Should be updated to ✅ Resolved.
2. `design-docs/rguilayout-portable-c-integration.md` code snippet (line 283) shows `#include "raygui.h"` (quoted, implies relative/local path) in example `raygui_impl.cpp` while actual file uses `<raygui.h>` (system include). Doc example should match implementation.

**Verdict:** ✅ APPROVED WITH NOTES — code and build changes are correct and complete; two doc-staleness items (non-blocking) should be cleaned up by whoever owns SUMMARY.md next.

**Reusable quality note:** When completing a "future build-integration task" that was documented as deferred, update the status in all referenced summary/integration documents at the same time. Stale "Future task" bullets become materially misleading for the next developer who opens those docs.

## Learnings

### 2026-04-29 — Root app/ui cleanup audit (read-only)

- **Still live (do not delete):** `ui_loader` load/map/cache/overlay functions are used by `game_loop.cpp` + `ui_navigation_system.cpp`; `text_renderer` is still runtime-critical for popup text and startup/shutdown font lifecycle; `ui_button_spawner.h` still drives title/level-select/paused/end-screen hit targets through `game_state_system.cpp`; `level_select_controller.cpp` is still wired through `input_dispatcher.cpp` listeners for Go/ButtonPress handling and diff-button re-layout.
- **Dead or test-only surface:** `spawn_ui_elements` path is fully removed; `ui_source_resolver.cpp/h` is no longer used by runtime code (tests only); `components/ui_element.h` types (`UIElementTag/UIText/UIButton/UIShape/UIDynamicText/UIAnimation`) have no runtime references; `text_width()` appears unused; `init_*_screen_ui()` entry points are defined but currently unused.
- **Regression-risk map:** deleting `ui_button_spawner` or `level_select_controller` now will break menu/level-select semantic input; deleting `ui_loader` caches/overlay helpers will break gameplay HUD cache, level-select cache, and paused dim overlay paths; deleting screen-controller dispatch in `ui_render_system` breaks title, level select, settings, HUD, paused, game over, song complete, tutorial.
- **Test migration note:** `tests/test_ui_spawn_malformed.cpp` is intentionally disabled legacy coverage and should be removed or rewritten to cache/controller paths; if `ui_source_resolver` is removed, rewrite/remove dependent tests (`test_ui_source_resolver.cpp`, `test_redfoot_testflight_ui.cpp`, `test_high_score_integration.cpp`, and song-complete resolver assertions in `test_game_state_extended.cpp`).

### 2026-04-29T08:05:08Z — Independent Root UI Cleanup Audit (Session)

**Task:** `kujan-root-ui-cleanup-audit` — Second-pass independent review; confirm live dependencies, identify remaining dead surface, enforce guardrails.

**Outcome:** ✅ Completed
- Confirmed live runtime dependencies (8 controllers, loader, text_renderer, button_spawner, level_select)
- Identified runtime-dead: `ui_source_resolver.*`, `ui_element.h`
- Flagged additional cleanup: stale comments, empty vendor dir
- Enforced guardrails: no adapter reintroduction, no legacy JSON/ECS loops, no standalone exports to committed tree
- Recommended targeted cleanup PR for remaining dead surface

**Orchestration log:** `.squad/orchestration-log/2026-04-29T08:05:08Z-kujan.md`

### 2026-04-29 — Review: Keyser legacy UI removal audit (`ui_loader` + `ui_button_spawner`)

- **Verdict:** ❌ Not complete; user directive not yet met.
- **Diff reviewed:** active working diff for `CMakeLists.txt`, `app/ui/ui_loader.*`, `app/systems/ui_navigation_system.cpp`, `app/systems/ui_render_system.cpp`, `tests/test_game_state_extended.cpp`, `tests/test_world_systems.cpp`.
- **What is correctly removed:** `spawn_ui_elements` runtime path, `app/ui/adapters/*`, `app/ui/vendor/*`, `app/ui/generated/standalone/*`, and `app/ui/raygui_impl.cpp` (implementation ownership moved to title screen controller TU).
- **Blocking gaps vs directive:**
  1. `ui_loader` remains runtime-critical (`game_loop.cpp`, `ui_navigation_system.cpp`, layout cache + overlay builders, JSON screen loading).
  2. `ui_button_spawner.h` remains live and still spawns invisible menu hitbox entities from `game_loop.cpp` and `game_state_system.cpp`.
  3. `game_state_routing.cpp`, `level_select_controller.cpp`, and many tests still depend on `MenuButtonTag/HitBox` semantics.
  4. Legacy JSON tests and comments remain (`test_ui_layout_cache.cpp`, `test_ui_element_schema.cpp`, `test_ui_loader_routes_removed.cpp`, `ui_state.h`, `ui_layout_cache.h` comments).
- **Regression risks if spawner is deleted now:** Title "TAP TO START" has no raygui click handler; level-select card/difficulty interactions are rendered but not raygui-hit-tested; deleting ECS hitboxes without replacement will strand those flows.
- **Verification run:** `cmake -B build -S . -Wno-dev && cmake --build build -j4 && ./build/shapeshifter_tests "~[bench]" && ./build/shapeshifter_tests "[ui]" && ./build/shapeshifter_tests "[gamestate]" && ./build/shapeshifter_tests "[level_select]"` — all pass (848/74/36/26 cases respectively).

### 2026-04-29 — Final gate: legacy UI runtime removal (`ui_loader` + menu hitbox spawner)

- **Verdict:** ✅ APPROVE
- Verified deleted and unreferenced from runtime/tests/CMake: `app/ui/ui_loader.cpp`, `app/ui/ui_loader.h`, `app/ui/ui_button_spawner.h`, `app/ui/ui_source_resolver.cpp`, `app/ui/ui_source_resolver.h`, `app/components/ui_element.h`.
- Verified no live runtime calls remain to legacy JSON loader/cache or invisible menu spawner APIs (`load_ui`, `ui_load_screen`, `build_ui_element_map`, `build_hud_layout`, `build_level_select_layout`, `ui_load_overlay`, `build_overlay_layout`, `spawn_*_buttons`, `destroy_ui_buttons`, `spawn_ui_elements` all absent).
- Verified controller-owned interaction coverage: title tap-to-start/settings/exit, level-select card+difficulty+start, paused resume/menu (no settings control in current paused layout), and game-over/song-complete restart/level-select/main-menu.
- Verified forbidden legacy surfaces remain absent: adapters, JSON/ECS UI renderer path, `app/ui/vendor`, committed standalone exports, `app/ui/raygui_impl.cpp`.
- Validation run: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests '~[bench]'` passed (`753` test cases, `2145` assertions).

### 2026-04-29T08:23:46Z — Legacy UI Removal Final Review & Approval

**Session:** Legacy UI Removal Final Wave  
**Task:** kujan-final-legacy-ui-removal (code review & validation)  
**Status:** ✅ COMPLETED & APPROVED

**Verification Summary:**
1. File deletion audit (6 files): ✅ All deleted, zero references remain
2. Runtime reference audit: ✅ All legacy loader/spawner calls absent
3. Screen-controller coverage: ✅ All required flows operational
4. Forbidden surfaces audit: ✅ All absent (adapters, JSON/ECS path, vendor, etc.)
5. Build & test validation: ✅ 753 tests pass, zero warnings

**Artifacts:**
- Orchestration log: `.squad/orchestration-log/2026-04-29T08-23-46Z-kujan.md`

**Approval Verdict:** User directive 2026-04-29T08:17:40Z fully satisfied. Ready for merge.

