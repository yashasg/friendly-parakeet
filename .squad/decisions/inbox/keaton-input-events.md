# Decision: Input Events Migration (#273 + #333)

**Author:** Keaton  
**Date:** 2026-04-27  
**Issues:** #273, #333

## Decision

Implement issues #273 and #333 together as they share the same code paths.

### #333 — Migrate InputEvent to entt::dispatcher Tier-1

`EventQueue` (hand-rolled fixed array) is deleted. `InputEvent` is now
enqueued via `disp.enqueue<InputEvent>()` and delivered via
`disp.update<InputEvent>()`. `gesture_routing_system` and `hit_test_system`
become dispatcher listeners (`gesture_routing_handle_input`,
`hit_test_handle_input`) registered in `input_dispatcher.cpp`.

**Cross-type enqueue safety:** Tier-1 listeners enqueue `GoEvent`/
`ButtonPressEvent` (different types from `InputEvent`). EnTT's
same-update delivery hazard only applies when a listener enqueues
the SAME type it listens to — so no one-frame latency is introduced.

### #273 — ButtonPressEvent semantic payload

`ButtonPressEvent.entity` (a live entity handle) is replaced with:
```cpp
struct ButtonPressEvent {
    ButtonPressKind kind;       // Shape or Menu
    Shape           shape;      // valid when kind == Shape
    MenuActionKind  menu_action; // valid when kind == Menu
    uint8_t         menu_index; // valid when kind == Menu
};
```
Semantic encoding happens at hit-test time in `hit_test_handle_input`.
Keyboard shortcuts encode directly in `input_system`. This eliminates
the dangling-handle risk described in issue #273.

## Rationale

- Entity handles in events create a temporal coupling: the entity
  must still be valid when the handler runs. With fixed-step
  accumulator multi-tick, the entity could theoretically be destroyed
  between enqueue and update.
- Semantic payloads are value types — safe to hold across ticks.
- Deleting `EventQueue` removes a redundant abstraction now that
  `entt::dispatcher` handles all event transport.

## Test helpers added

`test_helpers.h` gains:
- `push_input(reg, type, x, y, dir)` — enqueues an `InputEvent`
- `run_input_tier1(reg)` — calls `disp.update<InputEvent>()`
- `press_button(reg, entity)` — encodes semantic `ButtonPressEvent`
  from entity components and enqueues it

## Side effect

`CMakeLists.txt` glob extended to include `app/archetypes/*.cpp`
(pre-existing missing glob that prevented `player_archetype.cpp`
from linking into `shapeshifter_lib`).
