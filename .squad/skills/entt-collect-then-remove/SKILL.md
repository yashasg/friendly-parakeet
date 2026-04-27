# Skill: EnTT Collect-Then-Remove

**Domain:** ECS / EnTT safety  
**Applies to:** Any system that removes components present in its active view

## Problem

EnTT component pools use a packed array. When you call `reg.remove<C>(e)`
on an entity that is part of the active view's component C pool, EnTT
may swap-and-pop the pool entry, silently skipping or double-visiting
entities. This is undefined behaviour in the iteration contract.

## Solution

Use a **two-pass** approach:
1. **Read pass**: iterate the view, collect `entt::entity` (and any component
   copies you need) into a `static std::vector<Record>` (static avoids heap alloc).
2. **Mutation pass**: iterate the vector, apply all `reg.remove<>` / `reg.destroy()`.

```cpp
struct Record { entt::entity e; /* copy of needed component fields */ };
static std::vector<Record> buf;
buf.clear();   // clear() keeps capacity — no malloc in steady state

auto view = reg.view<A, B, C>();
for (auto [e, a, b, c] : view.each()) {
    // Non-structural mutations (ctx() lookups, score updates) are safe here.
    buf.push_back({e, a, b});
}
// View exhausted — safe to remove view components now.
for (auto& r : buf) {
    reg.remove<A>(r.e);
    reg.remove<B>(r.e);
}
```

## Bonus: structural view split

If an inner `any_of<T>` / `all_of<T>` branch is the primary discriminator,
split into two structural views instead:
- `reg.view<..., T>()`            — entities that have T
- `reg.view<...>(entt::exclude<T>)` — entities that do not have T

This gives EnTT better cardinality hints and eliminates per-entity branching.

## Reference

- scoring_system.cpp refactor (#315): miss pass + hit pass with structural split
- cleanup_system.cpp (#242): static vector collect pattern for destruction
