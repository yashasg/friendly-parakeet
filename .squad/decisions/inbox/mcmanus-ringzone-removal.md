# Decision: Remove RingZone code

**Author:** McManus  
**Date:** 2026-04-26  
**Status:** Proposed

## Context

RingZone was intended to track timing zone transitions for obstacles with proximity rings (ShapeGate, ComboGate, SplitPath). User directive: "ringzone is not a component either, we should just remove the ringzone code for now, it is broken."

## READ-ONLY Audit: Removal Surface

### Files to DELETE (3 files)
1. `app/components/ring_zone.h` — RingZoneTracker component definition
2. `app/systems/ring_zone_log_system.cpp` — Zone transition logging system
3. Design doc references (informational only; don't block removal)

### Files to EDIT (4 files)

#### 1. `app/systems/all_systems.h:73`
**Remove:**
```cpp
void ring_zone_log_system(entt::registry& reg, float dt);
```

#### 2. `app/game_loop.cpp:152`
**Remove from tick_fixed_systems():**
```cpp
ring_zone_log_system(reg, dt);
```

#### 3. `app/systems/session_logger.cpp`
**Line 5 — Remove include:**
```cpp
#include "../components/ring_zone.h"
```

**Lines 121-129 — Remove RingZoneTracker emplacement from session_log_on_obstacle_spawn():**
```cpp
// Emplace RingZoneTracker only on obstacles that have a ring visual
if (req) {
    reg.emplace<RingZoneTracker>(entity);

    float dist = pos ? (constants::PLAYER_Y - pos->y) : constants::APPROACH_DIST;
    session_log_write(*log, t, "GAME",
        "RING_APPEAR shape=%s obstacle=%u dist=%.0fpx",
        ToString(req->shape),
        static_cast<unsigned>(entt::to_integral(entity)), dist);
}
```

**Preserve:** OBSTACLE_SPAWN logging (lines 100-118) — no change needed.

#### 4. CMakeLists.txt (if present)
**Check for:** `ring_zone_log_system.cpp` in source lists.  
**Remove:** Any explicit references.

### Test Impact
**No test files reference RingZone, RingZoneTracker, RING_ZONE, or RING_APPEAR.**  
Zero test coverage → zero test breakage.

### Log Format Impact
Session logs will **lose**:
- `RING_ZONE` entries (zone transition logging)
- `RING_APPEAR` entries (ring visual spawn logging)

Session logs will **keep**:
- `OBSTACLE_SPAWN` entries (unaffected)
- `COLLISION` entries (unaffected)
- `BEAT` entries (unaffected)

## Validation Commands

```bash
# 1. Build (verify zero warnings policy)
cmake -B build -S .
cmake --build build 2>&1 | tee build_output.txt

# 2. Check for orphaned references
rg "RingZone|ring_zone" app/ tests/ --type cpp

# 3. Run tests
./build/shapeshifter_tests

# 4. Quick smoke test
./build/shapeshifter
# (Verify game starts, obstacles spawn, no crashes)
```

## Implementation Patch (Smallest Change)

**DELETE:**
- `app/components/ring_zone.h`
- `app/systems/ring_zone_log_system.cpp`

**EDIT `app/systems/all_systems.h`:**
- Remove line 73: `void ring_zone_log_system(entt::registry& reg, float dt);`

**EDIT `app/game_loop.cpp`:**
- Remove line 152: `ring_zone_log_system(reg, dt);`

**EDIT `app/systems/session_logger.cpp`:**
- Remove line 5: `#include "../components/ring_zone.h"`
- Remove lines 121-129: RingZoneTracker emplacement block in `session_log_on_obstacle_spawn()`

**No changes needed:**
- `session_logger.h` (no RingZone references)
- CMakeLists.txt (no explicit ring_zone_log_system.cpp entry found in glob patterns)
- Tests (zero coverage)

## Constraints Respected

✓ READ-ONLY audit (no edits made)  
✓ No commits/pushes  
✓ Smallest surgical removal  
✓ User directive: remove broken code, don't redesign  
✓ Keaton actively modifying component boundaries — this audit provides the blueprint for when worktree is clear

## Next Steps

1. Wait for Keaton's component-boundary work to complete
2. Apply this patch atomically
3. Run validation commands
4. Verify zero warnings on all platforms (Clang, MSVC, Emscripten)
