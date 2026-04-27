# Decision: EnTT-safe iteration pattern for scoring_system (#315)

**Author:** McManus  
**Date:** 2026-05-17  
**Issue:** #315

## Decision

Scoring_system now uses **collect-then-remove** across two structural views.
All `reg.remove<>` calls on view components (Obstacle, ScoredTag, MissTag)
are deferred until after the view iterator is exhausted.

## Pattern (reusable)

```cpp
// Read pass — collect into static buffer
static std::vector<Record> buf;
buf.clear();
auto view = reg.view<A, B, C>();
for (auto [e, a, b, c] : view.each()) {
    buf.push_back({e, /* copies of needed data */});
}
// Mutation pass — safe: view is exhausted
for (auto& r : buf) {
    reg.remove<A>(r.e);   // A is a view component — would be UB mid-loop
    reg.remove<B>(r.e);
}
```

## Structural view split

The old `any_of<MissTag>` per-entity branch is replaced by two structural views:
- Miss: `reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle>()`
- Hit: `reg.view<ObstacleTag, ScoredTag, Obstacle, Position>(entt::exclude<MissTag>)`

This matches the `collision_system` pattern and gives EnTT better cardinality
hints for pool iteration.

## Impact on other systems

Any system that removes components present in its own active view should apply
this pattern. Known candidates from the ECS audit (F1 was scoring_system —
now fixed). Collision_system already tags-only (emplace) and does not remove
during iteration — compliant.
