# Decision: RNGState used via ctx().find/emplace in obstacle_spawn_system

**Author:** McManus  
**Date:** 2026-04-27  
**Issue:** #248

## Decision

`obstacle_spawn_system` now draws all random values through `RNGState::engine` (std::mt19937) using `std::uniform_int_distribution`, accessed via `reg.ctx()`.

The lazy-init pattern used is:
```cpp
if (!reg.ctx().find<RNGState>()) reg.ctx().emplace<RNGState>();
auto& rng = *reg.ctx().find<RNGState>();
```

EnTT v3.16.0 context does NOT have `get_or_emplace` — this is the correct fallback.

`RNGState` is now also explicitly emplaced in `make_registry()` (test helpers) for clean test setup.

## Impact

- All other systems that want ECS-resident randomness should follow this same pattern.
- `std::rand()` / `std::srand()` are no longer used anywhere in game systems.
