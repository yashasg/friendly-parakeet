# Review Decision: Model Authority Revision (McManus)

**Author:** Kujan  
**Date:** 2026-05-18  
**Status:** ❌ REJECTED  
**Reviewed:** McManus revision of Kujan BF-1..BF-4  
**Next revision owner:** Keyser (McManus locked out on rejection)  
**Keaton:** Remains locked out.

---

## Verdict: FAIL

Two blocking findings. The revision resolves BF-1, BF-3, and BF-4 correctly. BF-2 is not fully resolved due to a double-scaling defect introduced by the owned-mesh construction.

---

## Blocking Findings

### RF-1 (CRITICAL) — Double-scale on owned mesh (BF-2 re-fail)

**File:** `app/gameobjects/shape_obstacle.cpp:124`

```cpp
model.meshes[0] = GenMeshCube(constants::SCREEN_W_F, height, dsz->h);
// → mesh vertices span ±SCREEN_W_F/2, ±height/2, ±depth/2
```

**File:** `app/systems/camera_system.cpp:268`

```cpp
om.model.transform = slab_matrix(pd.cx, oz.z + pd.cz, pd.width, pd.height, pd.depth);
// slab_matrix applies MatrixScale(pd.width, pd.height, pd.depth)
```

`slab_matrix` (line 215) uses `MatrixMultiply(MatrixScale(w, h, d), MatrixTranslate(...))`. This is designed for a **unit mesh** — it scales the mesh to the intended dimensions and then translates. The shared slab mesh confirms: `sm.slab = GenMeshCube(1.0f, 1.0f, 1.0f)`.

When `build_obstacle_model()` uses `GenMeshCube(SCREEN_W_F, height, dsz->h)`, the mesh vertices already span the full intended dimensions. Applying `slab_matrix(..., SCREEN_W_F, height, dsz->h)` then applies `MatrixScale(720, 30, 40)` again, yielding a bar that is 720² = 518,400 virtual units wide, 30² = 900 units tall, 40² = 1,600 deep — invisible and off-screen.

**Required fix (Keyser):**  
Change line 124 to use a unit cube:
```cpp
model.meshes[0] = GenMeshCube(1.0f, 1.0f, 1.0f);
```
`ObstacleParts.width/height/depth` remain unchanged; they correctly feed `slab_matrix` as the intended final dimensions. This is identical to the shared slab pattern.

---

### RF-2 (HIGH) — BF-2 regression test does not catch double-scaling

**File:** `tests/test_obstacle_model_slice.cpp:424–449`

The test labelled "BF-2: non-identity transform" only asserts that translation components `exp_tx`, `exp_ty`, `exp_tz` are non-zero:

```cpp
CHECK(exp_tx != 0.0f);
CHECK(exp_ty != 0.0f);
CHECK(exp_tz != 0.0f);
```

This test passes whether the scaling is correct or catastrophically wrong. It does **not** validate scale components. A test that exercises double-scaling would still pass because the center translation is non-zero regardless.

**Required fix (Keyser):**  
Add a test that verifies the scale entries of the produced matrix match expected final dimensions for a unit mesh. At minimum, invoke `slab_matrix` with known inputs and assert `mat.m0 ≈ pd.width`, `mat.m5 ≈ pd.height`, `mat.m10 ≈ pd.depth` (using raylib Matrix component indices for the diagonal scaling terms). Alternatively, verify that the matrix correctly maps a canonical unit corner vertex to the expected world-space position.

---

## What Passes — Do Not Regress

- **BF-1 ✅** — `LoadModelFromMesh` is gone from executable code; only appears in comments. Manual `RL_MALLOC` construction is in place. `UploadMesh` correctly omitted (raylib 5.5 GenMesh* already uploads). Comment guard adequate.
- **BF-3 ✅** — `app/systems/obstacle_archetypes.*` confirmed deleted. No stale `systems/obstacle_archetypes` includes in `app/` or `tests/`. ODR violation resolved.
- **BF-4 ✅** — `ObstacleParts` has six POD float fields `cx/cy/cz/width/height/depth`; `static_assert(!std::is_empty_v<ObstacleParts>)` in place. Populated correctly at spawn.
- **Headless safety ✅** — `IsWindowReady()` guard in `build_obstacle_model()`; `if (!om.owned) continue;` in camera section 1b; headless entities have no `ObstacleModel` → view skips them.
- **No double-draw ✅** — `draw_owned_models()` iterates `ObstacleModel + TagWorldPass`; `draw_meshes()` iterates `ModelTransform`; camera section 1b writes `om.model.transform` and does NOT emit `ModelTransform` for LowBar/HighBar. Paths are mutually exclusive.
- **RAII semantics ✅** — `ObstacleModel` copy-deleted, move correct with `owned=false` strip on source.
- **ObstacleModel default init ✅** — `owned=false`, null mesh/material pointers, headless-safe.
- **All tests pass ✅** — 2975/2975 per McManus validation.

---

## Re-review Conditions

1. `GenMeshCube(1.0f, 1.0f, 1.0f)` in `build_obstacle_model()` — unit mesh, consistent with shared slab.
2. At least one test that asserts matrix scale components equal expected dimensions (verifies no double-scale from slab_matrix on unit mesh).
3. Acceptance scans from Keyser's checklist must re-pass (they still pass for BF-1/3/4; BF-2 scan must be re-run after mesh fix).

Re-route to Kujan for re-review after Keyser revision.
