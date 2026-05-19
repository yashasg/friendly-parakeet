# Skill: EnTT ctx Singleton Eager-Init Pattern

**Domain:** C++ / EnTT ECS  
**Author:** Keaton  
**Date:** 2026-05-03

## Problem

Per-frame `reg.ctx().find<T>()` lookups in hot systems add measurable cost even when the singleton is guaranteed to exist after init. The "find-or-emplace" defensive fallback pattern:

```cpp
// ❌ Anti-pattern: lazy init in a hot system
auto* p = reg.ctx().find<T>();
if (!p) p = &reg.ctx().emplace<T>();
```

...causes benchmark regressions (measured: +27.7% scroll-10, +4.7% player_input) because the lookup cost is paid every frame.

## Solution: Emplace Once at Startup

**Rule:** All ctx singletons are emplaced exactly once at startup, either directly in `game_loop_init` or in a subsystem init function called unconditionally from it. Systems use `get<T>()` — no null checks, no fallback emplace.

### Step 1 — Emplace at startup

```cpp
// In game_loop_init (or a subsystem init it calls):
reg.ctx().emplace<MyConfig>();   // default-constructed
// — or —
reg.ctx().emplace<MyConfig>(MyConfig{ .field = value });
```

If one-time platform detection or GPU init is needed before the value is meaningful, put it in a dedicated `subsystem_init(entt::registry&)` function declared in the relevant system header and called from `game_loop_init`.

### Step 2 — Read in hot systems

```cpp
// ✅ Correct: guaranteed to exist, zero branch overhead
const auto& cfg = reg.ctx().get<MyConfig>();
```

### Step 3 — One-time init with side effects (e.g., platform detection)

```cpp
// In subsystem_init (same TU as the anonymous-namespace struct):
void my_system_init(entt::registry& reg) {
    auto& policy = reg.ctx().emplace<WebInputPolicy>();
#ifdef PLATFORM_WEB
    policy.prefers_touch = detect_touch_platform();
#else
    (void)policy;   // suppress unused-variable warning on native
#endif
}
```

Declare in `all_systems.h`, call once from `game_loop_init`.

## Exemptions

Scratch-pad ctx accumulator queues (e.g., `ScorePopupRequestQueue`) used as
per-frame intent buffers are intentionally optional — they legitimately use
find/emplace and are not "always-present singletons." Most former scratch-pad
ctx accumulators have since migrated to per-frame row tables via Fabian
Principle 3 (see #1627 `PendingEnergyEffectTag`, #1628 `*ExpiredTag` /
`PendingObstacleDespawnTag`, #1629 `Pending{Miss,Hit,NonScorableCleanup}ResolveTag`);
new scratch should default to row tables, not new ctx singletons.

## Measured Impact

| Benchmark | Before | After |
|---|---|---|
| scroll-10 entities | +27.7% regression | regression gone |
| full-frame typical | +9.3% regression | regression gone |
| player_input+movement | +4.7% regression | regression gone |

## Reference

- Decision: `.squad/decisions/inbox/keaton-singletons-eager-init.md`
- Canonical init point: `app/game_loop.cpp` → `game_loop_init`
- Example: `app/systems/input_system.cpp` → `input_system_init`
