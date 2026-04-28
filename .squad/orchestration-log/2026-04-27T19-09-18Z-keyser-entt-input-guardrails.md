# Orchestration Log: EnTT Input Model Guardrails

**Timestamp:** 2026-04-27T19:09:18Z  
**Agent:** Keyser (DoD/Architecture)  
**Event:** Completed pre-implementation guidance for EnTT input system refactor  
**Status:** DELIVERED

---

## Summary

Keyser completed diagnostics and published architecture guardrails for migrating the hand-rolled `EventQueue` to EnTT's `entt::dispatcher`. Document: `.squad/decisions/inbox/keyser-entt-input-model.md`

**Core recommendation:** Replace `EventQueue` fixed arrays with `dispatcher` stored in `reg.ctx()`. Use `enqueue`+`update` (deferred delivery) — **not** `trigger` (immediate). System execution order remains unchanged; dispatcher becomes the transport layer.

---

## Key Decisions

1. **Dispatcher placement:** `reg.ctx().emplace<entt::dispatcher>()`  
   - Consistent with existing singleton pattern (GameState, EventQueue, InputState, BurnoutState)
   - Avoids breaking `(entt::registry&, float dt)` system signature convention
   - Lifetime scoped to registry

2. **Event delivery:** `enqueue`+`update`, **not** `trigger`  
   - Preserves deterministic fixed-step behavior
   - Explicit control over delivery timing (two-tier: InputEvent before loop → GoEvent/ButtonPressEvent inside loop)
   - Satisfies no-replay invariant (#213)

3. **System order:** One frame structure preserved  
   - `input_system` → `disp.update<InputEvent>()` → `test_player_system` → fixed-step loop with `player_input_system`
   - Listeners run in registration order (enforce in `game_loop_init` block comment)

---

## Guardrails (7 risks documented)

- **R1:** Multi-consumer ordering enforced via registration order in `game_loop_init`
- **R2:** No overflow cap; verify test_player_system doesn't exceed prior MAX=8 per frame
- **R3:** `clear` vs `update` semantics clarified (defensive vs delivered)
- **R4:** Listener registry access via payload/lambda (no naked global ref)
- **R5:** No connect-within-handler (EnTT UB)
- **R6:** `trigger` prohibited for game input events; document in lint comment
- **R7:** Stale event discard across phase transitions (add Baer test)

---

## Recommended Migration Order

1. Add dispatcher to `reg.ctx()` (inert, zero-risk)
2. Migrate InputEvent tier (gesture_routing + hit_test as listeners)
3. Migrate GoEvent/ButtonPressEvent tier (player_input_system handlers)
4. Remove EventQueue struct
5. Baer gate: R7 test + no-replay validation

---

## Recipients (for cross-agent context append)

- Keaton (C++ Perf) — implementation sequencing
- Baer (Testing) — guardrail validation gates, R7 test
- McManus (Gameplay) — integration with player systems
