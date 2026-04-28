# Kujan Review Decision: ECS Cleanup Batch #273/#333/#286/#336/#342/#340

**Filed:** 2026-04-28  
**Reviewer:** Kujan  
**Branch:** user/yashasg/ecs_refactor

---

## Verdict: ✅ ALL SIX ISSUES APPROVED — CLOSURE-READY

---

## Per-Issue Findings

### #273 — ButtonPressEvent semantic payload (Keaton)

**APPROVED**

- `entt::entity` field removed; replaced with `{ButtonPressKind, shape, menu_action, menu_index}` value payload.
- Encoding at hit-test time in `hit_test_handle_input` and directly in `input_system` keyboard path — no stale-handle window.
- All consumers (player_input, game_state, level_select, test_player) updated.
- `test_event_queue.cpp` includes explicit stale-entity safety test (`[events]`); `test_hit_test_system.cpp` asserts semantic fields (kind/shape/menu_action).
- `test_helpers.h` `press_button()` / `drain_press_events()` helpers clean.

**Non-blocking notes:**
- `player_input_system.cpp` section heading "EventQueue consumption contract" (line ~97) is stale in title; body is accurate. Minor cleanup candidate.

---

### #333 — InputEvent entt::dispatcher Tier-1 migration and EventQueue deletion (Keaton)

**APPROVED**

- `EventQueue` struct fully deleted from `input_events.h` and `game_loop_init`.
- `input_system` correctly: R7 `disp.clear<InputEvent>()`, then `disp.enqueue<InputEvent>()`.
- `gesture_routing_handle_input` + `hit_test_handle_input` registered as Tier-1 sinks in `wire_input_dispatcher` in canonical order (R1).
- `unwire_input_dispatcher` disconnects all three sinks (InputEvent, GoEvent, ButtonPressEvent).
- Two-tier architecture documented in `input_dispatcher.cpp` block comment.
- `[R7]` tests pass: 10 assertions / 3 test cases. `[input_pipeline]`: 24 assertions / 10 test cases. `[dispatcher]`: 16 assertions / 6 test cases.
- `UIActiveCache` ctx addition in same commit is correctly placed in `game_loop_init`.
- Side-fix (CMakeLists `app/archetypes/*.cpp` glob) is correct and pre-existing; safe to include.

**Non-blocking notes:**
- R2 (per-frame cap): old `EventQueue.MAX=8` is gone; dispatcher has no built-in cap. Hardware input is naturally bounded; no regression observed. No explicit test documents the former ceiling — low risk but worth a follow-up note.
- `app/components/input.h` "Downstream systems should read EventQueue" comment is pre-existing stale (not introduced by this commit).

---

### #286 — SettingsState/HighScoreState helper extraction (Hockney)

**APPROVED**

- `SettingsState` is a plain data container; `audio_offset_seconds()` and `ftue_complete()` moved to `namespace settings` free functions in `settings_persistence.h/cpp`. ✅
- `HighScoreState` is a plain data container; all method bodies moved to `namespace high_score` free functions in `high_score_persistence.h/cpp`. ✅
- All call sites updated (`play_session.cpp`, all tests). Zero method-style calls remain on component structs.
- `[settings]`: 85 assertions / 18 test cases. `[high_score]`: 85 assertions / 16 test cases. Both pass.
- Architecture consistent with decisions.md and aligns with #313 approved `high_score` namespace.
- `<string>` retained in `high_score.h` for `HighScorePersistence::path` — correct.

---

### #336 — miss_detection_system exclude-view mutation regression tests (Baer)

**APPROVED**

- `tests/test_miss_detection_regression.cpp`: 5 test cases, 28 assertions — matches Baer's reported count.
- Covers: N expired → N MissTag+ScoredTag; idempotence (second run no-op); above-DESTROY_Y not tagged; pre-ScoredTag excluded; non-Playing phase no-op.
- No PLATFORM_DESKTOP guard — runs on all CI platforms.
- Live run confirms: **28 assertions / 5 test cases — all pass**.
- All four AC from issue #336 met.

---

### #342 — Non-platform-gated signal lifecycle tests (Baer)

**APPROVED**

- `tests/test_signal_lifecycle_nogated.cpp`: 6 test cases, 16 assertions.
- Covers: on_construct<ObstacleTag> increment; on_destroy decrement; wire_obstacle_counter idempotent; counter→0 after all destroy; no underflow; wired flag blocks re-entry.
- No PLATFORM_DESKTOP guard.
- Live run confirms: **16 assertions / 6 test cases — all pass**.
- All AC from issue #342 met.

**Non-blocking note:**
- Baer's decision inbox stated 23 assertions for this file; actual count is 16. Documentation discrepancy only; code is correct.

---

### #340 — SongState/DifficultyConfig ownership comments (McManus)

**APPROVED**

- Comments-only change. No behaviour changes.
- `SongState` correctly categorized into four groups: session-init, derived, per-frame mutable, beat-schedule cursor. System attributions (`song_playback_system`, `energy_system`, `beat_scheduler_system`) verified against actual `app/systems/` files — all exist.
- `DifficultyConfig` correctly annotated; `difficulty_system`, `obstacle_spawn_system` attributions verified.
- Notes the rhythm-mode skip behaviour accurately.
- No semantic errors in ownership attribution.

---

## Cross-Cutting Checks

| Check | Result |
|---|---|
| Full test suite (2808 assertions / 840 test cases) | ✅ All pass |
| Zero compilation warnings (`-Wall -Wextra -Werror`) | ✅ Keaton/Hockney verified |
| EventQueue struct removed from production code | ✅ Confirmed (3 stale doc references are comments, not functional code) |
| guardrails R1–R7 (decisions.md) | ✅ All met; R7 has live tests |
| No `trigger` usage for game input | ✅ Confirmed |
| No connect-in-handler (R5) | ✅ All connects in `wire_input_dispatcher` at init |
| `ButtonPressEvent.entity` fully removed | ✅ No remaining usage in app/ or tests/ |
| System names in #340 comments match actual files | ✅ Confirmed |

---

## Closure Guidance

All six issues are code-complete and test-verified. They may be closed:
- **#273**: Closure-ready.
- **#333**: Closure-ready.
- **#286**: Closure-ready.
- **#336**: Closure-ready.
- **#342**: Closure-ready.
- **#340**: Closure-ready.

### Non-blocking follow-up candidates (not blocking closure)

1. `player_input_system.cpp` section heading "EventQueue consumption contract" — rename to "dispatcher consumption contract".
2. `app/components/input.h` stale "read EventQueue" comment — pre-existing, update to reference `entt::dispatcher`.
3. Explicit per-frame InputEvent cap (R2) — document or test that dispatcher doesn't exceed former MAX=8 in practice.
4. Baer decision inbox assertion count (23 vs actual 16) — minor doc correction.
