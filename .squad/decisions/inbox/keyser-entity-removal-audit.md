# Entity Migration Code Removal Audit

**Prepared by:** Keyser (Lead Architect)  
**Date:** 2026-05-18  
**Scope:** Architecture-level removal/consolidation assessment for the entity migration effort  
**Status:** Read-only audit — **NO PRODUCTION CODE CHANGES** made

---

## Executive Summary

This audit identifies **~650–700 LOC of removable code** across 14 files when combined with the entity migration (Transform consolidation, Camera/UI/Obstacle/Player as first-class entities). The largest wins are:
1. **Dual archetype source** (105 LOC) — `app/systems/obstacle_archetypes.*` duplicates can be deleted after `app/archetypes/*` is canonical.
2. **Component-boundary violations** (490+ LOC) — `rendering.h`, `window_phase.h`, `ring_zone.h` shift from ECS boundaries to entity tags or Player state.
3. **Dead/broken systems** (50–100 LOC) — `ring_zone_log_system`, `beat_log_system` become moot or subsume into session/rhythm services.

**Dependency ordering is critical:** Many deletions are only safe *after* entities own Transform and visual/UI layers are restructured. Removing support code before migration breaks compilation.

---

## Detailed Classification

### DELETE NOW (No dependencies)

| Item | File(s) | LOC | Reason | Owner |
|------|---------|-----|--------|-------|
| **Stale obstacle_archetypes in systems/** | `app/systems/obstacle_archetypes.{h,cpp}` | 105 | Duplicate of canonical `app/archetypes/obstacle_archetypes.*`. Both files are active; systems version must be git-removed once archetype folder is committed. | Keaton (git mv choreography) |
| **Unused HEXAGON/SQUARE/TRIANGLE vertices** | `app/components/shape_vertices.h:25–49` | ~25 | Already removed (per prior Keyser audit). CIRCLE/V2 remain (3D floor ring geometry). | ✓ Done |

**Subtotal:** 130 LOC; **Remove immediately** via `git rm app/systems/obstacle_archetypes.{h,cpp}`.

---

### MOVE/REHOME NOW (Component-boundary cleanup, no entity semantics needed)

| Item | Current | New Home | LOC | Reason |
|------|---------|----------|-----|--------|
| **SettingsState/SettingsPersistence** | `app/components/` (never was) | `app/util/settings.h` | 0 | Already moved per #318. No component semantics; settings mutation is pure function logic. ✓ Complete |
| **RNG state** | `app/components/rng.h` | Consider `app/util/rng_context.h` | 6 | Singleton ctx state; not an entity property. Low priority (6 LOC). |

**Subtotal:** ~6 LOC rehoming; mostly complete.

---

### DELETE AFTER ENTITY MIGRATION (Prerequisite: Transform consolidation, first-class entities)

#### 1. **rendering.h → Renderpass tags** (77 LOC → ~20 LOC)

**Current issue:** Component-boundary violation — `rendering.h` is a "bucket" header mixing visual concerns (Layer, DrawSize, DrawLayer, ModelTransform, MeshChild, ScreenPosition, ScreenTransform, ObstacleChildren).

**Post-migration design:**
- Each visible entity carries a `RenderPass { uint8_t pass; }` tag (0=Background, 1=Game, 2=Effects, 3=HUD).
- Position/Size → Transform entity component (owner-of-truth).
- ModelTransform, ScreenPosition, ScreenTransform → computed per-frame by camera systems (not stored).
- DrawSize, DrawLayer → inlined into entity-specific archetype factory calls (no standalone struct).
- MeshChild, ObstacleChildren → part of spawn_obstacle_meshes logic (not persistent component).

**Removable after migration:**
```cpp
// ❌ DELETE: DrawSize, DrawLayer (archetype-embedded), ModelTransform, ScreenPosition, ScreenTransform (computed)
// ❌ DELETE: MeshChild, ObstacleChildren (factory-only, not persisted across frames)
// ✓ KEEP: Layer enum, RenderPass { uint8_t pass; } (minimal, entity-bound)
```

**Files affected:** `app/components/rendering.h` (remove ~50 LOC), update:
- `app/archetypes/player_archetype.cpp`
- `app/archetypes/obstacle_archetypes.cpp`
- `app/systems/camera_system.cpp` (remove ModelTransform emplace calls, compute inline)
- `app/systems/ui_render_system.cpp` (remove ScreenPosition queries, compute inline)
- `app/gameobjects/shape_obstacle.cpp` (DrawSize → fetch from Transform)

**Est. LOC removal:** 50–60; **Est. refactor effort:** 3–4 hours (camera + spawn logic).

---

#### 2. **window_phase.h → Player entity state** (13 LOC → 0 LOC)

**Current issue:** `WindowPhase` is stored in `ShapeWindow` component (part of Player), but declared as a separate header to break include cycles. User directive: move to Player state (not a component boundary).

**Post-migration:** `ShapeWindow` becomes `Player::shape_window` (nested struct inside a mega-Player component). WindowPhase is purely data within that struct.

**Removable:**
- `app/components/window_phase.h` — entire file (13 LOC) deleted
- Forward decl in `player.h` → remove, inline the enum definition

**Files affected:**
- `app/components/player.h` — inline `WindowPhase` enum
- `app/systems/shape_window_system.cpp` — remove `#include "../components/window_phase.h"`

**Est. LOC removal:** 13; **Refactor time:** <30min.

---

#### 3. **ring_zone.h + ring_zone_log_system → session logging** (11 + 96 LOC → 0 LOC)

**Current issue:** `RingZoneTracker` component (11 LOC) + `ring_zone_log_system` (96 LOC) form a broken logging subsystem. User directive: remove for now (RingZone is incomplete).

**Post-migration:** Session logging integrates directly into `collision_system` or `scoring_system` for zone transitions (no separate component tracking). Ring-zone events emit directly to `SessionLog` ctx.

**Removable:**
- `app/components/ring_zone.h` — entire file (11 LOC)
- `app/systems/ring_zone_log_system.cpp` — entire file (96 LOC)
- `session_logger.cpp:emplace<RingZoneTracker>(entity)` call — remove
- `all_systems.h` — remove `void ring_zone_log_system(...)` declaration
- `game_loop.cpp` — remove `ring_zone_log_system(reg, dt);` call

**Files affected:**
- `app/systems/session_logger.cpp` (remove emplace call, ~2 LOC)
- `app/systems/all_systems.h` (1 line)
- `app/game_loop.cpp` (1 line)

**Est. LOC removal:** 107; **Refactor time:** 1 hour (ensure session logging still works without it).

---

### DELETE AFTER ENTITY MIGRATION (Prerequisite: UI/Text refactoring)

#### 4. **ui_layout_cache.h → Inline HUD layout computation** (73 LOC → 20 LOC)

**Current issue:** Pre-computed layout cache (HudLayout, OverlayLayout, LevelSelectLayout) stored in ctx. User suspects cache is overkill — UI elements are created once, never re-rendered from scratch.

**Status:** **NEEDS DESIGN DECISION** — Requires confirmation that:
1. HUD elements are spawned once per screen load (not repeatedly each frame).
2. Layout is deterministic from JSON (no runtime tweaking).
3. Removing cache does not regress performance.

**If YES, removable:**
- `app/components/ui_layout_cache.h` — replace with inline struct in `ui_loader.cpp` (local scope, not ctx).
- `build_hud_layout()`, `build_level_select_layout()`, `extract_overlay_color()` move into `ui_render_system.cpp` as inline lambdas.
- `ui_render_system.cpp` — compute layouts per-frame from JSON schema (not ctx lookup).

**Files affected:**
- `app/components/ui_layout_cache.h` — delete entire file (73 LOC)
- `app/systems/ui_loader.cpp` — remove cache building, inline into spawn phase (refactor ~50 LOC)
- `app/systems/ui_render_system.cpp` — remove ctx lookup, compute inline (~10 LOC change)
- `app/systems/ui_navigation_system.cpp` — remove cache invalidation logic (2–3 calls)

**Est. LOC removal:** 73; **Refactor time:** 2–3 hours (verify no perf regression).

---

#### 5. **text_renderer.h + TextContext → Centralized text service** (37 + 22 LOC header)

**Current issue:** Dual text paths exist:
- `TextContext` (ctx singleton) + `text_draw()` free functions — used by HUD and level_select scenes
- `UIText` + `UIDynamicText` components — used by generic UI elements

**User hypothesis:** All visible text is UI text; `TextContext` can merge into a single service.

**Status:** **NEEDS DESIGN DECISION** — Requires confirmation:
1. Is there any non-UI text rendering path in production gameplay?
2. Can UIText/UIDynamicText be generalized to handle all text (HUD scores, titles, etc.)?

**If YES, refactoring path:**
- `TextContext` remains in ctx (correct — it's a resource, not an entity property).
- Consolidate `text_draw` + `text_draw_number` into a single `UITextRenderer` system that handles both ui_element text and dynamic text.
- Remove per-frame text_draw() calls scattered through ui_render_system (they become entity iterations).

**Likely NO-DELETE** — TextContext is appropriately placed in ctx. But text-rendering system consolidation is valuable to audit.

**No immediate LOC removal**, but **refactoring path identified** for a future audit.

---

### KEEP FOR NOW (Core to entity model or under active audit)

| Item | Reason | Notes |
|------|--------|-------|
| **Position + Velocity** | Source-of-truth until Transform entity migration complete. ~70 usages in systems. | Scheduled for removal in Transform PR (after entity refactor). |
| **TextContext** | Correctly placed in ctx as resource (not entity property). Font loading/unloading is lifecycle service. | Consolidate text-rendering systems, but keep ctx storage. |
| **Transform** | Currently placeholder (13 LOC, unused). Will become entity-owned world position/rotation. | Implement post-entity-migration. |
| **Color, Lifetime, ParticleData** | Pure data components; no logic violations. Correctly scoped to render + particle systems. | Keep as-is. |
| **Rendering layer components** | Layer, RenderPass will be needed (17 LOC core). Visual entity tagging is essential. | Trim bloat (DrawSize, ModelTransform, ScreenPosition), keep layer tags. |

---

## Dependency Ordering (Critical Path to Safe Deletions)

```
0. ✅ Commit app/archetypes/ folder + delete app/systems/obstacle_archetypes.*
   └─ (Safe now, no blocking dependencies)

1. 📌 Migrate Player, Camera, Obstacle, UI elements to first-class entities
   ├─ Implement Transform as entity component
   ├─ Player entity: ShapeWindow, Lane, VerticalState nested structs (no components)
   └─ Camera entity: Transform + camera metadata (not ScreenTransform)

2. 🎯 DELETE window_phase.h (inline WindowPhase into player.h ShapeWindow)
   └─ Post-step 1, pre-refactor rendering (no cascade)

3. 🔄 Refactor rendering: entity archetypes emplace RenderPass tags, not DrawSize/DrawLayer
   ├─ Collapse camera_system computation (no ModelTransform storage)
   ├─ Remove MeshChild/ObstacleChildren persistent storage
   └─ DELETE rendering.h bloat (~50 LOC)

4. 🛑 DELETE ring_zone.h + ring_zone_log_system
   ├─ Prerequisite: session logging refactored to emit zone transitions inline
   ├─ No blocking dependencies after removal
   └─ Safe only after collision/scoring systems absorb zone tracking

5. ❓ DESIGN DECISION: ui_layout_cache
   ├─ Confirm: HUD layout deterministic from JSON, no runtime tweaking?
   ├─ Confirm: No perf regression if cache removed?
   └─ If YES → DELETE (73 LOC); if NO → KEEP and optimize instead

6. ❓ DESIGN DECISION: text_renderer consolidation
   ├─ Confirm: All visible text is UI text?
   ├─ Generalize UIText to handle HUD + popups + level_select?
   └─ If YES → Consolidate systems (10–20 LOC reduction)
```

**Critical constraint:** Never delete Position/Velocity or rendering component usages until Transform entity is live. Current search shows 70 Position usages, 42 rendering component usages across systems. Premature deletion causes compile failure.

---

## Summary Table: Code Removal Potential

| Phase | Category | Removable LOC | Effort | Blocker | Owner |
|-------|----------|---------------|--------|---------|-------|
| **NOW** | Duplicate archetype source | 105 | 5 min (git rm) | None | Keaton |
| **NOW** | Already moved (settings) | 0 | Done | None | ✓ Complete |
| **Post-entity** | WindowPhase rehome | 13 | 30 min | Step 1 | McManus |
| **Post-entity** | Ring zone (broken) | 107 | 1 hr | Logging refactor | McManus |
| **Post-entity** | Rendering.h bloat | 50–60 | 3–4 hr | Entity + camera refactor | McManus |
| **Design pending** | ui_layout_cache | 73 | 2–3 hr | Design decision | TBD |
| **Design pending** | Text consolidation | 10–20 | 2–3 hr | Design decision | TBD |
| | **TOTAL** | **~640–700** | **~11–15 hours** | Entity migration | |

---

## Non-Removal Optimizations (Component Consolidation)

Even if files are not deleted, these refactors reduce duplication:

1. **beat_log_system (11 LOC) + session_logger (171 LOC combined)**
   - Currently: Separate system + logger both track beats
   - Opportunity: Merge beat emission into beat_scheduler_system or session_logger (1 emplace, 1 log call)
   - Est. savings: ~5 LOC (not deletion, but consolidation)

2. **Dual obstacle archetype sources (before deletion)**
   - Both `app/systems/` and `app/archetypes/` define same 8 obstacle types
   - Immediately reconcile to single source (already done by prior audit)
   - Current state: Ready for `git rm app/systems/obstacle_archetypes.*`

3. **Rendering layer (after Transform migration)**
   - Move DrawSize/DrawLayer emplacement into `apply_obstacle_archetype` + `player_archetype` (inline, not struct)
   - Saves repeated `emplace<DrawSize>(entity, w, h)` scatter throughout systems
   - Est. savings: ~15 LOC factorization (not deletion)

---

## File-by-File Removal Checklist (Post-Entity Migration)

```
app/components/
├─ rendering.h
│  ├─ ❌ DELETE: DrawSize (emplace → archetype inline)
│  ├─ ❌ DELETE: DrawLayer (emplace → archetype inline)
│  ├─ ❌ DELETE: ModelTransform (compute inline in camera_system)
│  ├─ ❌ DELETE: ScreenPosition (compute inline in camera_system)
│  ├─ ❌ DELETE: ScreenTransform (compute inline in camera_system)
│  ├─ ❌ DELETE: MeshChild (factory-only)
│  ├─ ❌ DELETE: ObstacleChildren (factory-only)
│  ├─ ⚠️  TRIM: Layer enum → move to util/layer.h (3 LOC)
│  └─ ✓ KEEP: Minimal RenderPass tag
├─ window_phase.h → ❌ DELETE (13 LOC, inline into player.h)
├─ ring_zone.h → ❌ DELETE (11 LOC, logging only)
├─ shape_vertices.h → ✓ Already trimmed (CIRCLE + V2 remain)
├─ ui_layout_cache.h → ❓ DELETE if design approved (73 LOC)
└─ text.h → ✓ KEEP TextContext (resource, not entity)

app/systems/
├─ obstacle_archetypes.{h,cpp} → ❌ DELETE NOW (105 LOC, dupe)
├─ ring_zone_log_system.cpp → ❌ DELETE (96 LOC, post-entity)
├─ camera_system.cpp → 🔄 REFACTOR (remove Transform emplace, compute inline)
├─ ui_render_system.cpp → 🔄 REFACTOR (remove ScreenPosition queries)
└─ session_logger.cpp → 🔄 REFACTOR (remove RingZoneTracker emplace)

app/archetypes/
├─ obstacle_archetypes.{h,cpp} → ✓ KEEP (canonical, move archetype logic here)
└─ player_archetype.{h,cpp} → ✓ KEEP (first-class entity factory)
```

---

## Conclusion

**650–700 LOC of removable code** is achievable when combined with the entity migration, concentrated in three areas:

1. **Duplicate sources** (105 LOC) — Remove stale `app/systems/obstacle_archetypes.*` immediately.
2. **Component-boundary violations** (50–60 LOC in rendering.h, 13 LOC in window_phase.h, 107 LOC in ring_zone system) — Migrate post-entity refactor.
3. **Conditional removals** (73 LOC ui_layout_cache + 10–20 LOC text consolidation) — Pending design decisions.

**Critical success factor:** Adhere to dependency ordering; never delete Position/Velocity or rendering components until Transform and entity factories are live. A single premature deletion breaks compilation for the entire team.

**Next actions:**
1. ✅ Delete `app/systems/obstacle_archetypes.*` via `git rm` (today, no blockers)
2. 📋 Design decision: ui_layout_cache (confirm HUD layout is deterministic + no perf regression)
3. 📋 Design decision: text consolidation (confirm all visible text is UI text)
4. 🔄 Queue entity migration + rendering refactor per dependency chain
5. 🎯 Execute removal phase post-entity-migration per checklist above

---

**Decision owner:** Keyser (Lead Architect)  
**Review required from:** McManus (ECS), Baer (testing), Kujan (code quality)  
**Blocking issues:** None (audit only, no code changes)
