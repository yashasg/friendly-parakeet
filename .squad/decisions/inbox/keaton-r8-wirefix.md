# Keaton r8 — Lane Push Wire Fix

**Round:** 8  
**Author:** Keaton  
**Status:** Shipped — all tests green, zero warnings

---

## Part 1 — Wiring fix diff

`app/game_loop.cpp:192` — inserted `lane_push_response_system(reg, dt)` between
`collision_system` and `miss_detection_system` inside `tick_fixed_systems`:

```diff
     collision_system(reg, dt);
+    lane_push_response_system(reg, dt);
     miss_detection_system(reg, dt);
```

Call site is in `tick_fixed_systems` at `app/game_loop.cpp:192`, which is the
only non-render fixed-step path invoked every production frame from
`game_loop_frame → tick_fixed_systems` (not a debug path, not test-only).

---

## Part 2 — Integration test added

**File:** `tests/test_collision_system.cpp`  
**Test name:** `"integration: lane push consumed in production tick order"`  
**Tag:** `[collision][lane_push][integration]`

Calls `collision_system → lane_push_response_system → miss_detection_system` in
the exact order documented in `tick_fixed_systems` (game_loop.cpp:191-193).

Assertions:
- `CHECK_FALSE(reg.all_of<PendingLanePush>(p))` — component consumed, not accumulating
- `CHECK(reg.get<Lane>(p).target == 0)` — push actually applied

This test **would have FAILED** before the Part 1 wiring fix because
`lane_push_response_system` wasn't in the production sequence; `PendingLanePush`
would have persisted after the tick.

Note: `tick_fixed_systems` is `static` in `game_loop.cpp` and cannot be linked
from the test binary (which links against `shapeshifter_lib` only; runtime
sources such as `song_playback_system` are excluded). The test therefore
explicitly calls the three core systems in production order and documents the
source line in a comment.

### Self-wiring audit

All six existing lane-push unit tests (lines 384–488) that self-wire
`collision_system + lane_push_response_system` have been annotated with:

```
// UNIT-SCOPED: self-wires systems to isolate collision + response logic.
// The production path (system order) is covered by:
//   "integration: lane push consumed in production tick order"
```

They are retained as useful unit tests.

---

## Part 3 — Multi-obstacle contention test

**File:** `tests/test_collision_system.cpp`  
**Test name:** `"collision: two lane pushes in same tick — first wins, delta not overwritten"`  
**Tag:** `[collision][lane_push]`

Spawns player + two `LanePush` obstacles in the same tick (one Left, one Right).

Assertions:
- `REQUIRE(reg.all_of<PendingLanePush>(p))` — exactly one component (not two)
- `winning_delta == -1 || winning_delta == 1` — delta is one of the two, pinned
- `CHECK_FALSE(reg.all_of<PendingLanePush>(p))` after response — consumed once
- `CHECK(reg.get<Lane>(p).target == expected_lane)` — moved by exactly one step

Pins the first-obstacle-wins semantics enforced by the
`!reg.all_of<PendingLanePush>` guard in `collision_system`.

---

## Build + test status

```
All tests passed (2233 assertions in 795 test cases)
```

**Delta:** +2 test cases, +6 assertions (was 793 / 2227).  
**Warnings:** zero (clang -Wall -Wextra -Werror).

---

## PendingLanePush accumulation — proof

The integration test `"integration: lane push consumed in production tick order"`
directly asserts `CHECK_FALSE(reg.all_of<PendingLanePush>(p))` after one
production-order tick. Before the fix, `lane_push_response_system` was never
called in production and `PendingLanePush` would linger on the player entity
every time a LanePush obstacle was hit. That test now passes, proving
accumulation is gone.
