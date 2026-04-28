### Hockney — render_tags.h folded into rendering.h

**Date:** 2026-04-28  
**Decision:** `app/components/render_tags.h` deleted. `TagWorldPass`, `TagEffectsPass`, `TagHUDPass` moved to the end of `app/components/rendering.h`.

**Rationale:** Per directive `copilot-directive-20260428T073749Z-no-render-tags-component.md` and Kujan gate `kujan-render-tags-gate.md` — no new component headers during cleanup passes. All three tags are empty structs with no dependencies, so folding them into an existing header is zero-cost.

**Impact:**
- Production includes (`game_render_system.cpp`, `shape_obstacle.h`, `shape_obstacle.cpp`): `render_tags.h` include removed; `rendering.h` already present.
- Tests (`test_obstacle_model_slice.cpp`): `render_tags.h` include replaced with comment; tags visible via `rendering.h`.
- Build: zero warnings, all tests pass (71/71 in `[render_tags][model_slice]`).

**No behavior change.** `TagWorldPass` still emplaced in `shape_obstacle.cpp` and queried in `draw_owned_models()`.
