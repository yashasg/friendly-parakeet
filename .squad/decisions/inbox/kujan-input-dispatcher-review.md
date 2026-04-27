# Decision: EnTT Dispatcher Input Model — Review Outcome

**Author:** Kujan  
**Date:** 2026-04-27  
**Scope:** EnTT dispatcher/sink input-model rewrite (commit 2547830, Keaton)

## Verdict: APPROVED

All 2419 assertions / 768 test cases pass. Zero build warnings.

## What Shipped

The migration is **functionally complete** for its stated scope: GoEvent and ButtonPressEvent are now fully delivered via `entt::dispatcher` stored in `reg.ctx()`. The hand-rolled `go_count`/`press_count`/`go_events[]`/`press_events[]` arrays are gone from EventQueue. The manual `eq.go_count = 0` anti-pattern that caused the #213 replay bug is eliminated — drain semantics of `disp.update<T>()` are the replacement.

### Architecture actually used (differs from decisions.md Tier-1 spec)

`decisions.md` specified: input_system enqueues InputEvent → `disp.update<InputEvent>()` fires gesture_routing and hit_test as listeners.

What shipped instead: gesture_routing_system and hit_test_system remain direct system calls (not listeners). They read the raw EventQueue (touch/mouse gesture buffer) and call `disp.enqueue<GoEvent/ButtonPressEvent>`. The dispatcher drains in the first fixed sub-tick via `game_state_system → disp.update<GoEvent/ButtonPressEvent>()`, firing all three listener chains (game_state, level_select, player_input handlers) atomically.

**This is architecturally equivalent for the accepted acceptance criteria.** No pool-order latency hazard applies because gesture_routing and hit_test are not inside a `disp.update()` chain. Same-frame behavior is preserved.

### EventQueue not fully removed

`EventQueue` struct is retained as a raw gesture shuttle (InputEvent[] only). Decisions.md migration step 4 ("Remove EventQueue struct") is NOT done. The raw-input layer (EventQueue) and semantic-event layer (dispatcher) remain separate. This is acceptable — the acceptance criteria scoped the migration to "Go/ButtonPress event delivery where intended."

## Guardrails for Future Dispatcher Work

1. **No start-of-frame `disp.clear<GoEvent/ButtonPressEvent>()`**: R7 from decisions.md is not explicitly addressed for the dispatcher queues. In practice, game_state_system drains within the same frame (it's the first fixed-tick system). If a frame skips the fixed tick, events accumulate until the next tick — currently benign but worth hardening.

2. **Redundant `disp.update()` calls are intentional**: game_state_system, level_select_system, and player_input_system all call `disp.update<GoEvent/ButtonPressEvent>()`. Only the first call per tick delivers events; subsequent calls are no-ops. The player_input_system redundancy is required for isolated unit tests that call it directly (test_player_input_rhythm_extended).

3. **Contract test comments are now stale**: `test_entt_dispatcher_contract.cpp` says "gesture_routing must use trigger(), not enqueue()". This was written before Keaton's approach was chosen. The actual implementation avoids the latency problem differently (direct system call, not listener). The comment should be updated to not mislead future readers.

4. **R4 — registry reference lifetime**: `disp.sink<GoEvent>().connect<&handler>(reg)` stores a pointer to `reg`. Safe because `unwire_input_dispatcher` disconnects before `reg.clear()`. Future changes that omit the unwire step would introduce a dangling pointer.
