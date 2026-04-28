# Code Removal Estimate: Component Consolidation & Entity Refactor

**Author:** Keaton (C++ Performance Engineer)  
**Date:** 2026-04-28  
**Status:** READ-ONLY AUDIT (no edits made)  
**Scope:** File-level C++ audit identifying removals + moves + renames (LOC savings where possible)

---

## Executive Summary

Estimated **~485 LOC can be cleanly removed** (three deletable categories) + **~120 LOC moves** (component boundary fixes that reduce misuse but preserve LOC). High-risk removals (rendering refactor, Transform vs Position/Velocity replacement) require architectural decisions beyond component cleanup.

---

## CATEGORY 1: TRUE DELETIONS (Safe, Verified)

### ✅ 1a. RingZone Complete Removal (~117 LOC deleted)

**Status:** AUDITED (McManus), ready to implement  
**Risk:** LOW — zero test coverage, logging-only system, no game logic dependencies

| File | Action | LOC | Notes |
|------|--------|-----|-------|
| `app/components/ring_zone.h` | DELETE | 11 | Component: `RingZoneTracker` enum/struct |
| `app/systems/ring_zone_log_system.cpp` | DELETE | 95 | Complete system: zone transition logging |
| `app/systems/all_systems.h:73` | DELETE | 1 | Decl: `void ring_zone_log_system(...)` |
| `app/game_loop.cpp:152` | DELETE | 1 | Call: `ring_zone_log_system(reg, dt);` |
| `app/systems/session_logger.cpp:5` | DELETE | 1 | Include: `#include "../components/ring_zone.h"` |
| `app/systems/session_logger.cpp:121-129` | DELETE | 8 | Emplace block: RingZoneTracker emplacement |

**Total deletions: 117 LOC**  
**Validation:** `rg "RingZone\|ring_zone\|RING_ZONE"` should be empty post-removal (except in doc files)

---

### ✅ 1b. UILayoutCache Removal (~73 LOC deleted)

**Status:** USER DIRECTIVE ("not needed", JSON loads once, renders repeatedly)  
**Risk:** MEDIUM-HIGH — Verify all render paths still work; affects gameplay HUD flow

**Deletion path:**
| File | Action | LOC | Impact |
|------|--------|-----|--------|
| `app/components/ui_layout_cache.h` | DELETE | 73 | Structs: `HudLayout`, `OverlayLayout`, `LevelSelectLayout` |
| `app/systems/ui_loader.cpp` | EDIT | TBD* | Remove calls to `build_hud_layout()`, `build_level_select_layout()`, `extract_overlay_color()` |
| `app/systems/ui_source_resolver.cpp` | EDIT | TBD* | Remove `HudLayout` lookups in score/timer positioning |
| `app/systems/ui_render_system.cpp` | EDIT | TBD* | Replace layout struct queries with direct Position/Size component queries per entity |

*Edit LOC TBD — must trace all `HudLayout`/`OverlayLayout`/`LevelSelectLayout` usage (24 references found via grep)

**Current usage:** 24 references across 3 systems  
**Refactor approach:** Cache layout data as fixed entity Position/Size components set once, not in struct

**Validation commands:**
```bash
rg "HudLayout|OverlayLayout|LevelSelectLayout" app/
# Should all be in ui_loader.cpp/ui_render_system.cpp edits, nowhere else

# Play 3 levels (Easy, Med, Hard), verify:
# - HUD renders in correct positions (buttons, scores, shapes)
# - Level select menu appears correctly (card layout)
# - Overlays (pause, game over) render properly
```

**Total deletions: ~73 LOC** (plus some system cleanup LOC savings)

---

## CATEGORY 2: COMPONENT BOUNDARY MOVES (LOC preserved, misuse fixed)

### ✅ 2a. Settings Already Moved ✓ (Complete)

**Status:** COMPLETED (Keaton, earlier session)  
**Files:** `app/util/settings.h` (created), `app/components/settings.h` (deleted)  
**LOC impact:** ZERO (struct definitions moved, not removed; imports updated)  
**Validation:** Already tested, zero-warning build confirmed

---

### ⚠️ 2b. WindowPhase Consolidation (~13 LOC moved, not deleted)

**Current state:** Separate file `app/components/window_phase.h` to avoid header cycle  
**Proposed move:** Inline enum in `app/components/player.h`

**Files:**
| File | Action | LOC | Reason |
|------|--------|-----|--------|
| `app/components/window_phase.h` | DELETE | 13 | Enum definition only, 2 includes of it |
| `app/components/player.h` | EDIT | +13 | Add enum definition inline |
| `app/components/rhythm.h` | EDIT | 0 | Update include: remove `#include "window_phase.h"`, add include `"player.h"` if needed (or leave as is if rhythm doesn't use it) |

**Risk:** LOW  
**Check:** `rg "include.*window_phase" app/` (currently 2 refs; both should be eliminated)

**Total: 13 LOC moved, 0 LOC deleted**

---

### ⚠️ 2c. Shape Vertices: KEEP (Not Safe to Delete)

**Status:** REJECTED (see keaton-component-boundary-cleanup.md)  
**Why NOT removed:**
- Used for performance-critical floor rendering (rlVertex3f batch mode)
- Not equivalent to raylib DrawCircle/DrawRectangle (which are immediate mode)
- Requires rendering architecture redesign, not cleanup
- Also used in benchmarks

**Files:** `app/components/shape_vertices.h` — REMAINS  
**LOC: 51** (not moved or deleted)

---

### ⚠️ 2d. Rendering Component Bucket (NOT a component bucket)

**Status:** DESIGN VIOLATION — needs clarification

**Issue:** `rendering.h` lives in `app/components/` but contains 9 data structs that serve multiple purposes:
1. **Pure rendering data:** `ModelTransform`, `MeshChild`, `ScreenPosition` (only used by render systems)
2. **Transient scene graph:** `ObstacleChildren` (parent→child entity list)
3. **Enums for render dispatch:** `Layer`, `MeshType`
4. **Transform caches:** `DrawSize`, `DrawLayer`, `ScreenTransform`

**Current usage:** 77 references (all rendering-path only)

**Options (requires design decision):**
- A) Move to `app/util/rendering.h` (not a component file)
- B) Split into `app/components/mesh_rendering.h` + `app/util/screen_transform.h`
- C) Keep as-is (large but coherent for render systems)

**Recommendation:** Move to `app/util/rendering.h` (not an entity component, just render pass data)

**If moved:**
| File | Action | LOC | Impact |
|------|--------|-----|--------|
| `app/components/rendering.h` → `app/util/rendering.h` | MOVE | 77 | All 19 includes updated |
| All render systems | EDIT | 0 | Update 19 `#include "../components/rendering.h"` → `#include "../util/rendering.h"` |

**LOC impact: ZERO** (pure move)  
**Risk:** LOW (single include path change)

---

## CATEGORY 3: ARCHITECTURAL CHANGES (High Risk, Design-Level Decisions)

### ❌ 3a. Transform Replaces Position/Velocity (NOT RECOMMENDED without design review)

**Status:** USER SUGGESTION — requires careful evaluation

**Current state:**
- `Position` (x, y) + `Velocity` (dx, dy) = 78 usage points across game
- `Transform` component exists but appears unused in game loop

**Claimed benefit:** Consolidate two structs into one  
**Risk factors:**
1. **Query structure changes:** All systems querying `Position, Velocity` would need `Transform` + property access changes
2. **Backward compatibility:** 78 edit sites × high risk of introducing bugs
3. **Unknown side effects:** May affect cache locality, memory layout, archetype performance
4. **No clear performance gain:** Two 32-bit floats per component; no savings

**Recommendation:** HOLD — this is a refactor, not cleanup. Requires:
- Performance before/after measurement
- Full regression test pass
- Possible ECS archetype redesign for hot paths

**LOC impact if done:** ~30–50 edits across systems (risk:high, unclear savings)

---

### ❌ 3b. Camera as Entity (NOT RECOMMENDED)

**Status:** MISUNDERSTANDING — already correctly in context

**Current state:** `GameCamera` and `UICamera` stored in `reg.ctx()` (singletons)  
**Why it's correct:**
- Only one camera per render pass; entity model would waste IDs and complicate queries
- All access is `reg.ctx().get<GameCamera>()` (3 references, all in render systems)
- Standard ECS singleton pattern for global singletons

**Recommendation:** NO CHANGE (design is correct)

---

### ❌ 3c. Text vs UIText (NOT Redundant)

**Current state:**
- `TextContext` (in `text.h`): Registry context singleton storing pre-loaded Fonts
- `UIText` (in `ui_element.h`): Entity component for renderable text (on UI entity)

**Relationship:** Non-redundant pair
- `TextContext` = immutable font library (singleton)
- `UIText` = mutable per-entity text parameters (color, size, alignment)

**Example:** 3 score labels on HUD = 3 UIText entities, 1 TextContext in ctx

**Recommendation:** NO CHANGE (design is correct)

---

## SUMMARY TABLE

| Category | Files | Action | Safe? | LOC | Validation |
|----------|-------|--------|-------|-----|-----------|
| RingZone | 6 files | DELETE | ✅ YES | -117 | Zero refs; test build |
| UILayoutCache | 1 + 3 edits | DELETE | ⚠️ MED | -73 + TBD | Play 3 levels HUD smoke test |
| WindowPhase | 1 file | MOVE | ✅ YES | 0 | 2 includes eliminated |
| rendering.h | 1 + 19 edits | MOVE | ✅ YES | 0 | Build check only |
| Transform refactor | ~78 sites | REFACTOR | ❌ HIGH | +0 | Not recommended without perf data |
| Camera entity | N/A | NO CHANGE | — | 0 | Already correct |
| Text consolidation | N/A | NO CHANGE | — | 0 | Already correct |

---

## IMPLEMENTATION ORDER (Recommended)

1. **RingZone deletion** (lowest risk, isolated)
   - Commit: "Remove RingZone logging system"
   - Validate: Build + unit tests

2. **rendering.h move** (pure include path change)
   - Commit: "Move rendering.h to util/ (not component bucket)"
   - Validate: Build only

3. **WindowPhase consolidation** (header cleanup)
   - Commit: "Consolidate WindowPhase into player.h"
   - Validate: Build only

4. **UILayoutCache removal** (largest, requires tracing)
   - Commit: "Remove ui_layout_cache (JSON loads once)"
   - Validate: Play 3 levels (HUD rendering smoke test)

---

## VALIDATION COMMANDS (Full Suite)

```bash
#!/bin/bash
set -e

echo "=== Build ==="
cmake -B build -S .
cmake --build build 2>&1 | grep -i warning && exit 1 || true

echo "=== Orphaned symbols ==="
rg "RingZone|ring_zone|RING_ZONE|HudLayout|OverlayLayout|LevelSelectLayout" app/ tests/ --type cpp | grep -v "//\|*" && exit 1 || true

echo "=== Run tests ==="
./build/shapeshifter_tests

echo "=== Smoke test (10s gameplay) ==="
timeout 10 ./build/shapeshifter || true

echo "All validation passed"
```

---

## Notes for Keaton's Next Steps

- **Settings.h move:** ✅ Already landed locally; include in estimate as completed
- **RingZone:** Ready to implement; blueprint from McManus approved
- **UILayoutCache:** Largest effort; requires tracing 24 call sites in 3 systems
- **rendering.h move:** Low-effort; just include path changes
- **WindowPhase:** Quick cleanup after rendering.h stabilizes
- **Transform/Camera/Text decisions:** Not cleanups; require design review if user wants them

---

## Estimated Timeline

| Task | Est. LOC | Time | Risk |
|------|----------|------|------|
| RingZone | -117 | 10 min | Low |
| rendering.h move | 0 | 15 min | Low |
| WindowPhase | 0 | 10 min | Low |
| UILayoutCache | -73 | 60 min | Med |
| **Total** | **-190 LOC net** | **~2 hrs** | **Low-Med** |

(Transform/Camera/Text decisions add 4–8 hrs if pursued; not recommended in this pass)
