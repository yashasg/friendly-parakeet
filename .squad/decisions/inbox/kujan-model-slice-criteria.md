# Review Criteria — Model/Transform Obstacle Slice (First Slice)
**Author:** Kujan  
**Date:** 2026-05-18  
**Status:** ACTIVE — gates Keaton's implementation once obstacle-data cleanup (current diff) is merged  
**Scope:** LowBar obstacle as the first kind migrated to owned multi-mesh `Model` + `Model.transform` authority

---

## Input Artifact Baseline (Current Diff)

The current diff (45 files changed) is Keaton's obstacle-data cleanup:
- `obstacle_data.h` and `obstacle_counter.h` deleted; `RequiredShape`, `BlockedLanes`, `RequiredLane`, `RequiredVAction` moved into `obstacle.h`; `ObstacleCounter` moved to `obstacle_counter_system.h`
- `#include` references updated across 40+ files
- No production logic changed — pure header-boundary rehome

**This cleanup is the prerequisite baseline.** The slice review below assumes it has landed.

---

## Blocking Checklist

Each item below is a gate. All must be PASS before approval.

---

### B1 — `on_destroy<Model>` listener registered before first spawn

- [ ] `camera::init` (or `game_loop_init`) connects the `on_destroy<Model>` listener.
- [ ] Listener calls `UnloadModel(m)` — safe here because obstacle `meshes[i]` are all entity-owned.
- [ ] Listener guards against zero-init: `if (m.meshes) UnloadModel(m);`
- [ ] **No** `UnloadModel` call at session/pool shutdown for obstacle entities. Unload happens at entity destruction only.

**Why blocking:** A missing listener means GPU memory is never released. A listener registered after the first spawn misses entities already alive. An unguarded call on a default-init `Model` is UB.

---

### B2 — No `LoadModelFromMesh` in obstacle path

- [ ] Zero calls to `LoadModelFromMesh` in `obstacle_archetypes.cpp`, `shape_obstacle.cpp`, or any helper called from the obstacle spawn path.
- [ ] `grep -r LoadModelFromMesh app/` returns no hits in the obstacle path.

**Why blocking:** `LoadModelFromMesh` copies the mesh *pointer* into `model.meshes[0]`, not the data. Calling `UnloadModel` on such a Model frees the shared mesh from `ShapeMeshes`, corrupting all other drawables.

---

### B3 — One entity, three entity-owned meshes

- [ ] LowBar (and any other kind in this slice) spawns as a **single entity** with one `Model` component.
- [ ] `model.meshCount == 3` (primary shape mesh + 2 slabs, or layout-appropriate for the kind).
- [ ] All three meshes created via `GenMesh*` + `UploadMesh` on this entity — **not** pointers into `camera::ShapeMeshes`.
- [ ] `model.meshes`, `model.materials`, `model.meshMaterial` allocated with `RL_MALLOC`; `meshCount`, `materialCount` set correctly before `UploadMesh`.
- [ ] No `MeshChild` entities created for the migrated kind. No `ObstacleChildren` component. `spawn_obstacle_meshes` is NOT called for Model-owned kinds.

**Why blocking:** Pointer aliasing into `ShapeMeshes` breaks the unload contract. Child-entity creation is exactly what this slice eliminates.

---

### B4 — Explicit part descriptors stored on entity

- [ ] Each obstacle entity carries a typed part-descriptor component (e.g., `ObstaclePartDesc` or similar) that records the per-slab/per-shape geometry data (x, w, h, d) **alongside** the `Model`.
- [ ] Part descriptors are emplaced by `apply_obstacle_archetype`, not only baked silently into mesh vertex positions.
- [ ] At minimum: enough data for `scroll_system` to recompute `model.transform` each frame without re-reading raw mesh vertex buffers.

**Why blocking:** The scroll system must rewrite `model.transform` per frame (per `keyser-model-contract-amendment.md §3`). Without stored geometry, it cannot reconstruct the origin matrix from scratch. `slab_matrix(x, z, w, h, d)` requires `x`, `w`, `h`, `d` to be available at scroll time.

---

### B5 — `Model.transform` is the sole world-space authority

- [ ] Model-owning obstacle entities do **not** carry a `Position` component.
- [ ] Model-owning obstacle entities do **not** carry a `ModelTransform` component.
- [ ] `game_camera_system` does NOT compute a `ModelTransform` for Model-owning entities. The existing `ObstacleTag + Position + ...` view in `game_camera_system` must either be guarded or restructured to skip Model-owning kinds.
- [ ] `scroll_system` (rhythm path) writes `model.transform = obstacle_origin_matrix(...)` per frame.
- [ ] Per-mesh vertex offsets are baked at spawn time relative to obstacle origin; `model.transform` moves the whole obstacle in world space.

**Why blocking:** Two sources of truth (Position + model.transform) will silently diverge. The contract from `copilot-directive-20260428T052611Z` is explicit: `Model.transform` IS the world-space matrix for Class A entities.

---

### B6 — `cleanup_system` correctly destroys Model-owning obstacles

- [ ] `cleanup_system` currently queries `ObstacleTag, Position` — this will silently **skip** obstacles that no longer carry `Position`.
- [ ] Either: `cleanup_system` adds a second view for `ObstacleTag, Model` entities (using Z-position derived from `model.transform.m14` or part-descriptor), OR the migrated kind retains a lightweight `WorldPos` (Class B pattern) for scroll tracking only.
- [ ] Escaped obstacles must not accumulate silently in the registry.

**Why blocking:** Silent missed-destroy leads to ECS pool growth, corrupted BeatInfo state, and eventual out-of-bounds. This is the highest-risk integration point.

---

### B7 — `TagWorldPass` emplaced; render system queries it

- [ ] `apply_obstacle_archetype` (or the migration wrapper) emplaces `TagWorldPass` on Model-owning obstacle entities.
- [ ] `game_render_system` has a `reg.view<Model, TagWorldPass>()` draw loop that calls `DrawMesh(model.meshes[i], model.materials[model.meshMaterial[i]], model.transform)` for each mesh index.
- [ ] The existing `ModelTransform`-based draw loop remains intact for non-migrated kinds.

**Why blocking:** Without `TagWorldPass` wiring, the migrated obstacle is invisible at runtime.

---

### B8 — Zero warnings, clean build (native + Emscripten)

- [ ] `cmake --build build` completes with **zero** compiler warnings (`-Wall -Wextra -Werror`).
- [ ] No implicit `void*` → `Mesh*` or `void*` → `Material*` cast warnings from `RL_MALLOC` (cast explicitly).
- [ ] No unused variables from the old `MeshChild`/`Position` paths left behind `#ifdef` guards or dead branches.
- [ ] Emscripten build (WASM) passes with same flag set.

**Why blocking:** Zero-warnings is a hard project policy.

---

### B9 — Test coverage

- [ ] `test_obstacle_archetypes.cpp` updated for migrated kinds: `REQUIRE(reg.all_of<Model, TagWorldPass>(e))`, `CHECK(!reg.all_of<Position>(e))`, `CHECK(!reg.all_of<ModelTransform>(e))`, `CHECK(reg.get<Model>(e).meshCount == 3)`.
- [ ] Destruction test: spawn a Model-owning obstacle entity, call `reg.destroy(e)`, verify no crash and `on_destroy<Model>` fired (can be inferred by checking Valgrind / AddressSanitizer clean run).
- [ ] Full test suite (`./build/shapeshifter_tests "~[bench]"`) passes. Baseline is 862 test cases (post-cleanup diff).
- [ ] No test uses `LoadMaterialDefault()` / `GenMesh*` without an `InitWindow` guard or mock path — headless test scaffolding must be verified.

**Why blocking:** Model GPU lifecycle in headless tests is the biggest CI risk. A crash in tests masks correctness signal.

---

## Pre-Identified Risks (Non-Blocking but Must Be Addressed Before Final Approval)

| # | Risk | Mitigation |
|---|---|---|
| R1 | `cleanup_system` silent bypass | See B6. Must be resolved, not deferred. |
| R2 | EnTT copies `Model` struct on `emplace` | Use `reg.emplace<Model>(entity, std::move(m))` — verifiable with static_assert or in test. |
| R3 | `LoadMaterialDefault()` in headless tests | Confirm test scaffolding calls `InitWindow` before spawn, or use a lightweight mock material path behind a `#ifdef TEST` guard. |
| R4 | `model.meshMaterial` null init | Explicitly memset or set each `meshMaterial[i] = 0` after `RL_MALLOC` — `RL_MALLOC` does not zero-init. |
| R5 | `game_camera_system` double-writes transform | If an obstacle kind is present in BOTH the old `Position`-based view and the new `Model` view, `game_camera_system` will emplace a `ModelTransform` onto it and the render system draws it twice. Guard or restructure. |
| R6 | `slab_matrix` TRS encoding | Z-scroll updates must call `slab_matrix(x, new_z, w, h, d)` — patching `m14` alone corrupts the scale encoding. Confirm `scroll_system` does a full matrix recompute, not a partial patch. |

---

## Out of Scope for This Slice

- Player entity migration (separate slice, borrow-pointer + partial unload path)
- ShapeGate / ComboGate / SplitPath migration (subsequent slices after LowBar/HighBar pattern is proven)
- `Transform.h` / `Position` → `WorldPos` rename (deferred; not required to unblock this slice)
- `ObstacleChildren` / `MeshChild` removal for non-migrated kinds (leave intact)

---

## Approval Condition

**APPROVED** when all B1–B9 items are checked, all R1–R6 risks have documented mitigations in the PR, and the full test suite passes clean.
