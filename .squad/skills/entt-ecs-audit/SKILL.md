# Skill: EnTT ECS Audit Checklist

**Reusable by:** Kujan (review), Keyser (diagnostics), any agent reviewing C++ ECS code.

## What This Is
A checklist for auditing EnTT-based C++ ECS codebases against the EnTT guide and DoD/ECS conventions.

---

## Checklist

### 1. Component purity
- [ ] All components are plain data structs with no logic methods (no game logic in `get_x()`, `update()`, etc.)
- [ ] Helper methods on components are limited to: computed accessors, clamping, key-building (pure transforms of own data, no registry access)
- [ ] No component header references `entt::registry` except for signal wiring utilities — if signal wiring is present, flag as a boundary question

### 2. System signatures
- [ ] Systems are free functions with signature `void system_name(entt::registry& reg, ...)` (non-const for writers, const for pure readers)
- [ ] Render systems and other read-only systems should take `const entt::registry&` — catches accidental mutations at compile time
- [ ] No system stores state in class fields; all mutable state lives in `reg.ctx()` or as entity components

### 3. Mutation during iteration (CRITICAL — UB risk)
Per EnTT guide §"What is allowed and what is not":
- [ ] Removing/emplacing components on the **current** entity during `view.each()` — **allowed** but invalidates refs; safe only if those refs are not accessed after the mutation
- [ ] Removing/destroying **other** entities' iterated components during iteration — **NOT allowed**, UB
- [ ] Pattern to flag: `reg.remove<T>(entity)` inside a loop iterating `<T, ...>` — verify no ref to T is used post-remove
- [ ] Preferred patterns: collect-then-destroy (see `cleanup_system.cpp`), or defer removes to end of loop body with `continue`

### 4. Constant duplication
- [ ] Check for `constexpr float X = N` defined in multiple `.cpp` files — should be in a shared header (e.g., `constants.h`)
- [ ] Known hotspot: `COLLISION_MARGIN` and `APPROACH_DIST` in this codebase

### 5. Context variables vs. components
- [ ] Singletons that are never iterated belong in `reg.ctx()`, not as components on a dedicated entity
- [ ] Verify all singletons are emplace'd in `game_loop_init` and not scattered across systems

### 6. Static locals as state
- [ ] `static` locals inside system functions = state outside the registry
- [ ] Acceptable if: intentional performance optimization, documented, cleared before use every call
- [ ] Problematic if: influences behavior between calls (e.g., a static frame counter that doesn't reset)

### 7. View correctness
- [ ] Exclusion filters (`entt::exclude<ScoredTag>`) are correct and account for all tagging paths
- [ ] Views that should lead on the smallest pool do so (no accidental performance reversals)
- [ ] No `reg.get<T>` inside a view loop when `T` is already in the view — use `view.get<T>(entity)` or structured bindings

### 8. Signal / observer correctness
- [ ] `on_construct` / `on_destroy` listeners are disconnected before `reg.clear()` in shutdown
- [ ] Listeners do not remove the triggering component from within themselves (UB per guide)
- [ ] Reactive storage: disconnected from observed pools before storage is destroyed

### 9. Hierarchy / child ownership
- [ ] Parent–child entity relationships use a component (e.g., `ObstacleChildren`) — not raw pointers
- [ ] Destruction order: child storage pool must be initialized before parent signal is connected (lower pool index = destroyed last in reverse-order teardown)

---

## Severity Rubric
| Severity | When to assign |
|----------|---------------|
| HIGH | Active UB path, silent data corruption, crash-on-trigger |
| MEDIUM | Dormant UB path (safe today, becomes UB with plausible future edit), drift-risk duplicated constants, broken contract on a named decision |
| LOW | Convention violation, const-correctness miss, organizational blurring of component/system boundary |
| NOISE | Intentional deviation with documentation, known trade-off, pure optimization opportunity |
