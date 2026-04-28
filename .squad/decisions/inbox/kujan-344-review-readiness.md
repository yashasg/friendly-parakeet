# Kujan — #344 Review Readiness Gate

**Date:** 2026-04-27  
**Issue:** #344 — Audit and consolidate dead or duplicate ECS systems  
**Scope:** P0/P1 pre-commit review gate  
**Status:** PRE-FINAL — blocker identified; cannot approve until resolved

---

## What I Reviewed

Keaton's working-tree changes (uncommitted against HEAD `f8a694b`):

| File | Change |
|------|--------|
| `app/archetypes/obstacle_archetypes.h/cpp` | NEW — canonical factory (moved from `app/systems/`) |
| `app/archetypes/player_archetype.h/cpp` | NEW — canonical player factory |
| `app/systems/obstacle_archetypes.h/cpp` | DELETED — old location |
| `CMakeLists.txt` | `ARCHETYPE_SOURCES` glob added, wired into `shapeshifter_lib` |
| `app/systems/beat_scheduler_system.cpp` | Updated include path |
| `app/systems/obstacle_spawn_system.cpp` | Updated include path |
| `app/systems/play_session.cpp` | Routes through `create_player_entity` |
| `tests/test_helpers.h` | All obstacle helpers route through canonical archetype |
| `tests/test_obstacle_archetypes.cpp` | Updated include path |
| `tests/test_player_archetype.cpp` | NEW — 7 tests for player archetype contract |

---

## Acceptance Criteria (Final Gate)

### AC1 — Canonical obstacle helper path ✅ PASS
- `app/archetypes/obstacle_archetypes.h/cpp` exists as the sole owner
- Old `app/systems/obstacle_archetypes.*` deleted
- Both callers (`beat_scheduler_system.cpp`, `obstacle_spawn_system.cpp`) include `archetypes/obstacle_archetypes.h`
- No stale include to old `app/systems/` path remains anywhere

**Evidence:** Confirmed by `git status` and grep. Both production callers updated.

### AC2 — No color drift in tests ✅ PASS
- All `make_*` obstacle helpers in `test_helpers.h` route through `create_obstacle_base` + `apply_obstacle_archetype`
- Old per-kind hardcoded colors (`{255,255,255,255}` for ShapeGate, `{0,200,200,255}` for LanePush, `{255,180,0}` for both bars) are gone
- `test_obstacle_archetypes.cpp` tests specific color values against the canonical factory directly

**Evidence:** Diff verified. All 7 obstacle helpers now delegate to the archetype.

### AC3 — No duplicated obstacle base contract ✅ PASS
- No dual construction path exists
- `create_obstacle_base` handles the shared pre-bundle (ObstacleTag, Velocity, DrawLayer)
- `apply_obstacle_archetype` handles kind-specific components
- Switch statement covers all 8 `ObstacleKind` values exhaustively

**Evidence:** `obstacle_archetypes.cpp` switch reviewed. No missing case.

### AC4 — Player archetype correctness ⚠️ BLOCKER — see below

### AC5 — CMake wiring ✅ PASS
- `file(GLOB ARCHETYPE_SOURCES app/archetypes/*.cpp)` added
- `${ARCHETYPE_SOURCES}` added to `shapeshifter_lib` static library
- `app/` is PUBLIC include root on `shapeshifter_lib` — all consumers resolve paths correctly
- Both `player_archetype.cpp` and `obstacle_archetypes.cpp` are picked up automatically

**Evidence:** CMakeLists.txt diff confirmed.

### AC6 — Warning-free build and test evidence ⏳ PENDING
- Full build must complete with zero warnings (`-Wall -Wextra -Werror`)
- All `[archetype]` and `[archetype][player]` tests must pass
- Full suite must pass (≥810 test cases)
- Pre-existing raylib linker warning is out of scope

**Evidence:** Build/test run required from Keaton/Baer against current working tree.

### AC7 — No P2/P3 scope creep ✅ PASS
- Change is focused on: archetype relocation + player archetype factory + test helper canonicalization
- LaneBlock→LanePush vestigial mapping in random spawner is unchanged (noted, deferred)
- `ObstacleArchetypeInput.mask` comment gap is unchanged (doc-only, deferred)

---

## BLOCKER — AC4: `make_player` not routing through `create_player_entity`

**Severity:** BLOCKING

**Finding:** `make_player` in `tests/test_helpers.h` has its own inline implementation that uses default constructors:

```cpp
reg.emplace<PlayerShape>(player);   // current = Shape::Circle (default)
reg.emplace<ShapeWindow>(player);   // target_shape = Shape::Circle, phase = Idle (default)
```

`create_player_entity` explicitly initializes:

```cpp
ps.current  = Shape::Hexagon;
ps.previous = Shape::Hexagon;
sw.target_shape = Shape::Hexagon;
sw.phase        = WindowPhase::Idle;
```

**Impact:** Any test using `make_player` creates a Circle player in center lane, while production **always** creates a Hexagon player (via `play_session.cpp → create_player_entity`). This divergence means freeplay tests exercise a different initial state than production.

`make_rhythm_player` correctly routes through `create_player_entity`. `make_player` does not.

**Required fix:** `make_player` must either:
- Route through `create_player_entity` (preferred — single source of truth), or
- Be removed in favor of direct `create_player_entity` usage in the tests that call it

**Who owns this revision:** Keaton (P0/P1 implementer)

---

## Non-Blocking Observations

1. **`make_player` comment** says "Hexagon in rhythm mode" but the implementation gives Circle. This comment was accurate for `make_rhythm_player` only — the existing comment is misleading.

2. **`test_player_archetype.cpp` coverage** is good — 7 tests covering component set, position, initial shape, ShapeWindow state, DrawLayer, DrawSize, and Lane/VerticalState. Covers the contract fully.

3. **Dead-surface regression check (SKILL applied):** No deleted test cases were identified that tested preserved behavior without migration. Test helper simplification (deletion of hardcoded construction blocks) is replaced by delegation to the canonical archetype — semantically equivalent. No symmetric branch gaps visible in the deleted test helper code.

---

## Pre-Final Verdict

**CANNOT APPROVE.** One blocker remains: `make_player` does not route through `create_player_entity`, creating a production–test initial-state divergence.

**Next action:** Keaton revises `make_player` to route through `create_player_entity`, then Baer runs build + full suite. Route completed diff + evidence back to Kujan for final gate.
