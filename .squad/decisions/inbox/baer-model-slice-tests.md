# Baer Decision — Model Slice Test Coverage
**Author:** Baer  
**Date:** 2026-04-28  
**Status:** ACTIVE — test files written, ready for review  
**Executors:** Keaton (must resolve BLOCKER-1/2/3 before enabling Section C)

---

## 1. What Was Done

Added deterministic test coverage for the Model/Transform obstacle migration slice:

### Files Added
| File | Tests today | Tests after Slice 1 |
|---|---|---|
| `tests/test_model_component_traits.cpp` | 6 (all pass) | 6 (unchanged) |
| `tests/test_obstacle_model_slice.cpp` (Section A+B) | 6 + 2 = 8 (all pass) | — replaced by Section C |
| `tests/test_obstacle_model_slice.cpp` (Section C) | 0 (guarded `#if 0`) | 7 (pending blockers) |

**Total new tests active now:** 14 (12 distinct test cases + 2 render-tag tests enabled from Section B)

---

## 2. Tests Active Today

### `test_model_component_traits.cpp`
- `Model` is `trivially_copyable` + `standard_layout` (static_assert — always active)
- `Model{}` has null pointers and zero counts
- `on_destroy<Model>` listener fires before entity removed (component readable in listener)
- `on_destroy<Model>` null-mesh guard skips UnloadModel branch (`meshes == nullptr`)
- `on_destroy<Model>` non-null-mesh pointer enters UnloadModel branch
- Listener fires exactly once per entity destruction (not zero, not twice)

### `test_obstacle_model_slice.cpp` — Section A (pre-migration baseline)
- LowBar, HighBar, LanePushLeft, LanePushRight all have `Position`, no `Model`
- No archetype emplaces `Model` on any of the 8 obstacle kinds

**These Section A tests will FAIL after Keaton's Slice 1** — that failure is the intended signal that Section C should be enabled.

### `test_obstacle_model_slice.cpp` — Section B (render-pass tags — ACTIVE)
- `TagWorldPass`, `TagEffectsPass`, `TagHUDPass` type-trait static_asserts (empty, standard-layout, distinct)
- EnTT pool isolation: each tag occupies a separate pool; emplacing one does not create the others

---

## 3. Blockers for Section C

**BLOCKER-1 — `slab_matrix()` in render_matrix_helpers.h:**  
`slab_matrix()` is currently `static` in `camera_system.cpp`. Scroll transform tests (asserting `model.transform.m14` after a scroll update) require it to be in `app/util/render_matrix_helpers.h`. Owner: Keaton.

**BLOCKER-2 — `ObstaclePartDescriptor` struct definition:**  
User directive requires obstacle entities to carry explicit part descriptors. The exact struct (field names, `count` semantics) is TBD. Section C tests are placeholder stubs; field access commented out. Owner: Keaton.

**BLOCKER-3 — `LoadMaterialDefault()` in headless tests:**  
After Slice 1, `apply_obstacle_archetype` will call `LoadMaterialDefault()` per entity. This requires an OpenGL context (`InitWindow`). Section C tests that call `apply_obstacle_archetype` for Model-carrying kinds CANNOT run in headless Catch2 without a material stub or integration harness. Options:
- (a) Run those specific tests under `InitWindow` in a dedicated integration binary.
- (b) Provide a stub `LoadMaterialDefault()` that returns a zeroed `Material{}` for test builds.
- (c) Separate `apply_obstacle_archetype` material creation into a distinct step callable without GPU.

Decision is Keaton's to make; Baer will write the tests once the strategy is confirmed.

---

## 4. Validation Commands

```bash
# Today — validates Section A + B + model_traits + model_lifecycle:
cmake -B build -S . -Wno-dev
cmake --build build --target shapeshifter_tests
./build/shapeshifter_tests "[model_traits],[model_lifecycle],[pre_migration],[render_tags]" --reporter compact
# Expected: 12 test cases, 42 assertions, all pass

# After Slice 1 implementation:
# 1. Remove the #if 0 guard in Section C of test_obstacle_model_slice.cpp
# 2. Confirm Section A tests now FAIL (correct — they should be deleted/replaced)
# 3. Run:
./build/shapeshifter_tests "[post_migration],[model_slice]" --reporter compact
# Expected: 7+ test cases pass (pending BLOCKER-3 resolution)

# Full suite regression gate:
./build/shapeshifter_tests "~[bench]" --reporter compact
```

---

## 5. Production Code Unchanged

No production source files were modified. Changes are entirely additive:
- `tests/test_model_component_traits.cpp` (new)
- `tests/test_obstacle_model_slice.cpp` (new)
