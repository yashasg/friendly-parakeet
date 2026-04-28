### Kujan — Reviewer Gate Addendum: render_tags.h Surface
**Date:** 2026-04-28  
**Artifact:** Model authority cleanup slice (current revision — Keyser owns)  
**Status:** ❌ BLOCKING — must be resolved before this slice is accepted

---

## Gate Requirement

`app/components/render_tags.h` **must not exist as a new committed file** when this cleanup slice lands.

**Authority:** Directive `copilot-directive-20260428T073749Z-no-render-tags-component.md` (captured 2026-04-28T07:37:17Z):

> "Do not add new component cleanup surfaces like `app/components/render_tags.h` during this cleanup pass; the pass is about removing/consolidating ECS/component clutter, not adding more component headers."

---

## Evidence

`render_tags.h` is currently **untracked** (`git status: ??`). The file defines three empty tag structs:

```cpp
struct TagWorldPass   {};
struct TagEffectsPass {};
struct TagHUDPass     {};
```

Production usage:
- `TagWorldPass` — emplaced in `shape_obstacle.cpp:147`; queried in `game_render_system.cpp:159` (`draw_owned_models` view)
- `TagEffectsPass` — **no production use** (test-only static_asserts)
- `TagHUDPass` — **no production use** (test-only static_asserts)

Included by: `game_render_system.cpp`, `camera_system.cpp` (comment only), `shape_obstacle.h`, `shape_obstacle.cpp`, `tests/test_obstacle_model_slice.cpp`

---

## Required Fix

**Fold all three tag structs into `app/components/rendering.h`.**

`rendering.h` already owns all render-related component types (`ModelTransform`, `MeshChild`, `ObstacleChildren`, `ObstacleModel`, `ObstacleParts`, `ScreenPosition`, `DrawLayer`, `DrawSize`, `ScreenTransform`). Three empty tag structs belong there, not in a new header.

Steps:
1. Append `TagWorldPass`, `TagEffectsPass`, `TagHUDPass` to `rendering.h` (no new includes needed — they have no dependencies).
2. Delete `app/components/render_tags.h`.
3. Replace all `#include "../components/render_tags.h"` (and `#include "components/render_tags.h"` in tests) with `#include "../components/rendering.h"` (or `components/rendering.h` respectively).
4. Verify zero new warnings; build must pass clean.

---

## Narrow Acceptable Alternative

If the implementer has a technical reason not to use `rendering.h`, the tags may be folded into any **already-existing** component header — provided:
- The chosen header is already included by all consumers.
- No new component header file is created.
- `TagEffectsPass` and `TagHUDPass` are either included (for future use) or removed if the test assertions covering them are also removed — no dead-code stubs in production.

**There is no acceptable path that leaves `render_tags.h` as a new committed file.**

---

## Ownership

- **Revision owner:** Keyser (owns the double-scale fix cycle for this artifact)
- **Locked out:** Keaton, McManus (current revision cycle)
- This addendum does not require a new review cycle — it is a gate extension on the existing Keyser revision.

---

## Tests

`tests/test_obstacle_model_slice.cpp` guards the tag presence via static_asserts gated behind `#if 1 // SLICE0_RENDER_TAGS_ADDED`. Once the structs move into `rendering.h`, update the include path in the test. The static_assert logic is correct and must be preserved.
