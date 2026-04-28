# Component Cleanup Audit — ECS Boundary Map

**By:** Keyser (Lead Architect)  
**Date:** 2026-05-19  
**Type:** Read-only audit — no code changes. Priority map for implementation.

---

## Audit Methodology

Every file in `app/components/` was checked against three criteria:
1. Does it define data that belongs to an entity (not a registry context singleton)?
2. Is it the canonical definition (not a duplicate)?
3. Is it correctly placed (not a utility or constant table)?

---

## Priority Map

### P0 — DELETE (dead duplicates, zero consumers)

| File | Verdict | Canonical Location |
|------|---------|-------------------|
| `app/components/audio.h` | **DELETE** — byte-for-byte duplicate | `app/systems/audio_types.h` |
| `app/components/music.h` | **DELETE** — byte-for-byte duplicate | `app/systems/music_context.h` |

**Evidence:** Every file that needs `SFX`, `AudioQueue`, `SFXBank`, or `MusicContext` includes
the `systems/` path. Zero files include `components/audio.h` or `components/music.h`.
These were created during model-slice cleanup and never removed when the systems/ canonical
versions were confirmed. Deleting them closes an ODR latency risk.

---

### P1 — MERGE into obstacle.h (valid components, wrong files)

| File | Contents | Action |
|------|----------|--------|
| `app/components/obstacle_counter.h` | `ObstacleCounter{count, wired}` | Merge into `obstacle.h`, delete file |
| `app/components/obstacle_data.h` | `RequiredShape`, `BlockedLanes`, `RequiredLane`, `RequiredVAction` | Merge into `obstacle.h`, delete file |
| `app/components/ring_zone.h` | `RingZoneTracker{last_zone, past_center}` | Merge into `obstacle.h`, delete file |

**Rationale:** All three files define entity components that belong to the obstacle domain and
are already co-authored with `ObstacleTag`/`Obstacle` in `obstacle.h`. The split exists only
because of incremental migration work; no architectural reason justifies separate files.

**Include-site impact for each merge:**
- `obstacle_counter.h`: `app/systems/obstacle_counter_system.cpp`
- `obstacle_data.h`: `app/archetypes/obstacle_archetypes.cpp`, `app/gameobjects/shape_obstacle.cpp`, `app/systems/ring_zone_log_system.cpp`, `app/systems/session_logger.cpp`, `app/systems/collision_system.cpp`, `app/systems/beat_scheduler_system.cpp`, `app/systems/game_state_system.cpp`, `app/systems/test_player_system.cpp`, `app/systems/ui_render_system.cpp`, `app/systems/camera_system.cpp`, `app/systems/obstacle_spawn_system.cpp`
- `ring_zone.h`: `app/systems/ring_zone_log_system.cpp`, `app/systems/session_logger.cpp`, `app/systems/all_systems.h`, `app/game_loop.cpp`

All include sites update to `#include "../components/obstacle.h"` (or `"components/obstacle.h"`
from game_loop/all_systems).

**Note on ring_zone removal directive:** The prior directive said to remove ring-zone code
entirely, but it was never actioned. `RingZoneTracker` is currently live in two systems.
Decision: merge to obstacle.h now (reduces file count); if ring_zone_log_system is to be
removed, that is a separate system-scope decision for the team.

---

### P2 — RELOCATE (valid data, wrong folder)

| File | Contents | Action |
|------|----------|--------|
| `app/components/settings.h` | `SettingsState`, `SettingsPersistence` | Move to `app/util/settings.h`; update `app/util/settings_persistence.h` include |
| `app/components/shape_vertices.h` | `constexpr` vertex arrays in `shape_verts::` namespace | Move to `app/util/shape_vertices.h`; update `game_render_system.cpp` include |
| `app/components/ui_layout_cache.h` | `HudLayout`, `OverlayLayout`, `LevelSelectLayout` | Move to `app/systems/ui_layout_cache.h`; update `ui_loader.h` include |

**Rationale:**
- `SettingsState`/`SettingsPersistence` live in `reg.ctx()`, not on entities. `app/util/`
  already holds `settings_persistence.h` which includes `components/settings.h` — that
  relationship is backwards. The data belongs in util next to its persistence code.
- `shape_verts` is a compile-time constant table used by one renderer. It is not a component.
  It belongs in `app/util/` alongside other non-entity math helpers.
- `HudLayout`/`OverlayLayout`/`LevelSelectLayout` are reg.ctx() layout caches built and
  consumed entirely within UI systems. They belong in `app/systems/`. Longer term, the
  directive says these should be eliminated by converting to proper UI entities; relocation
  to systems/ is step one.

**Include-site impact:**
- `settings.h`: `app/util/settings_persistence.h`, `app/systems/player_input_system.cpp`,
  `app/systems/scoring_system.cpp`, `app/systems/game_state_system.cpp`,
  `app/systems/haptic_system.cpp`, `app/systems/ui_source_resolver.cpp`,
  `app/systems/player_movement_system.cpp`, `app/game_loop.cpp`
- `shape_vertices.h`: `app/systems/game_render_system.cpp` only
- `ui_layout_cache.h`: `app/systems/ui_loader.h` (re-export); `app/systems/ui_render_system.cpp`,
  `app/systems/ui_navigation_system.cpp` (consumers)

---

### CONFIRMED OK — No action

| File | Status |
|------|--------|
| `app/components/render_tags.h` | **Does not exist.** Tags (TagWorldPass/TagEffectsPass/TagHUDPass) already live at the bottom of `rendering.h`. Directive: do not create this file. |

---

## Accidental Reintroductions from Model Slice

The `audio.h` and `music.h` files in `components/` are the direct result of the model-slice
migration creating files in `components/` in parallel with the authoritative `systems/`
definitions. They were never cleaned up. This is the same pattern that caused `render_tags.h`
to be flagged — work that spawns new files in `components/` without a post-pass audit.

**Pattern to block:** Any subagent doing migration work must not add files to `app/components/`
without explicit architect approval and a confirmed include-site switch.

---

## Execution Order for Implementer

1. DELETE `app/components/audio.h` and `app/components/music.h` (P0, zero risk)
2. MERGE P1 files into `obstacle.h`, update all include sites, delete the three source files
3. RELOCATE P2 files in order: settings.h → util, shape_vertices.h → util, ui_layout_cache.h → systems
4. Build + test clean after each step (zero-warning policy enforced)
