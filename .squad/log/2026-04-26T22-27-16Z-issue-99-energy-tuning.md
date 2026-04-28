# Issue #99: Energy Bar tuning forgiveness

**Timestamp:** 2026-04-26T22:27:16Z  
**Squad:** Game Designer (target), Ralph/Coordinator (implementation), Kujan (review), Scribe (logging)

## Scope

Resolved TestFlight balance concern that Energy Bar drain/recovery was too punishing for the intended forgiveness model.

## Outcome

- Defined target miss forgiveness: 8 consecutive misses from full survives at ~0.04 energy; the 9th miss depletes.
- Tuned constants:
  - `ENERGY_DRAIN_MISS = 0.12f`
  - `ENERGY_DRAIN_BAD = 0.06f`
  - `ENERGY_RECOVER_OK = 0.03f`
  - `ENERGY_RECOVER_GOOD = 0.06f`
  - `ENERGY_RECOVER_PERFECT = 0.12f`
- Preserved the Energy Bar as sole death authority; misses and Bad timing drain, Ok/Good/Perfect recover, and `energy_system()` owns GameOver.
- Updated `energy-bar.md` and `game.md` with the new values, forgiveness target, examples, and TestFlight QA scenarios.
- Added deterministic survivability tests for the target miss streak and cancellation invariants.

## Coordinator tightening

- Used the Game Designer handoff to lock the concrete survivability target.
- Added tests for 8-miss survival, Perfect canceling Miss, and Good canceling Bad.
- Updated the death-model test to use the new 8+1 miss depletion cadence.
- Fixed stale `game.md` forgiveness-table values found during review.

## Validation

- `git diff --check` passed for the #99 files.
- Focused tests passed: 129 assertions in 73 test cases.
- Full native non-benchmark suite passed: 2586 assertions in 785 test cases.

## Review

Kujan approved after the stale forgiveness table was corrected.

## GitHub

Comment posted to issue #99.
