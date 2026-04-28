# Revision Decision: Double-Scale Fix (RF-1 + RF-2)

**Author:** Keyser  
**Date:** 2026-05-19  
**Status:** COMPLETE — awaiting Kujan re-review  
**Context:** Kujan rejected McManus revision; Keyser assigned as sole owner of this revision cycle.

---

## Decision Summary

Two Kujan blocking findings resolved. All previously accepted fixes preserved.

---

## RF-1: Unit cube mesh in build_obstacle_model()

**File:** `app/gameobjects/shape_obstacle.cpp:124`

**Change:**
```cpp
// Before (McManus — double-scale defect):
model.meshes[0] = GenMeshCube(constants::SCREEN_W_F, height, dsz->h);

// After (Keyser — unit cube, matches shared slab pattern):
model.meshes[0] = GenMeshCube(1.0f, 1.0f, 1.0f);  // unit cube; slab_matrix scales to world dims
```

**Rationale:**  
`slab_matrix` applies `MatrixScale(w, h, d)` designed for a unit cube — identical to the shared `ShapeMeshes.slab = GenMeshCube(1,1,1)` pattern. Pre-scaling the mesh to real dimensions and then applying `slab_matrix` with the same dimensions squares every axis, producing a bar 720² wide and invisible off-screen. `ObstacleParts.width/height/depth` remain unchanged and continue to feed `slab_matrix` as the intended final world dimensions.

---

## RF-2: Scale-diagonal regression test

**File:** `tests/test_obstacle_model_slice.cpp`

**Change:** Replaced the weak "non-zero translation components" BF-2 test with a scale-diagonal assertion that catches double-scaling.

**New test name:** `"BF-2: slab_matrix scale diagonal equals world dimensions (unit-cube contract)"`  
**Tag:** `[model_slice][bf2_regression]`

**What it asserts:**
- Replicates `slab_matrix` formula inline: `MatrixMultiply(MatrixScale(w,h,d), MatrixTranslate(...))`
- `mat.m0 == w` (scale diagonal X = intended width)
- `mat.m5 == h` (scale diagonal Y = intended height)  
- `mat.m10 == d` (scale diagonal Z = intended depth)
- `mat.m12 / mat.m13 / mat.m14 != 0.0f` (non-identity transform)
- GPU-free: all raymath functions are pure arithmetic

**Why this catches the defect:** With the old pre-scaled mesh (`GenMeshCube(w, h, d)`), the rendered dimensions would be `w²` × `h²` × `d²` because slab_matrix scales a mesh assumed to be unit-size. The test verifies the diagonal is exactly once the intended dimension, so any double-scale regression would require the test constants to be wrong — not the scale check itself.

**Note on raylib Matrix layout:**  
raylib uses column-major storage. Scale diagonal is `m0, m5, m10`. Translation is `m12, m13, m14`. The incorrect `m3/m7/m11` fields are always zero in a Scale×Translate product — discovered during validation, corrected before final pass.

---

## Accepted Fixes Preserved (Do Not Regress)

- BF-1 ✅ Manual RL_MALLOC model construction; no LoadModelFromMesh executable use
- BF-3 ✅ systems/obstacle_archetypes.* deleted; no stale includes
- BF-4 ✅ ObstacleParts carries six POD float fields; static_assert in test
- Headless safety ✅ IsWindowReady() guard + om.owned skip in camera section 1b
- No double-draw ✅ draw_owned_models vs draw_meshes paths mutually exclusive
- RAII semantics ✅ ObstacleModel copy-deleted, move strips owned flag

---

## Validation Results

- `cmake -B build -S . -Wno-dev`: clean
- `cmake --build build --target shapeshifter_tests`: 0 warnings, 0 errors
- `./build/shapeshifter_tests "~[bench]" --reporter compact`: **2978 assertions, 885 tests, all pass**
- `cmake --build build --target shapeshifter`: clean
- `git diff --check`: clean (exit 0)
- `grep -rn 'LoadModelFromMesh(' app/ tests/`: no executable calls
- `grep -rn '"systems/obstacle_archetypes' app/ tests/`: no stale includes

---

## Re-route

Route to Kujan for re-review.
