# Issue #82: rhythm-spec and energy-bar docs contradict MISS semantics

**Timestamp:** 2026-04-26T21:58:33Z  
**Squad:** Ralph (implementation), Coordinator (tightening/validation), Kujan (review), Scribe (logging)

## Scope

Resolved documentation drift around whether MISS causes instant GameOver or drains the Energy Bar.

## Outcome

- Recorded Energy Bar / `EnergyState` as the sole death authority.
- Documented that MISS drains `ENERGY_DRAIN_MISS` and never directly requests GameOver.
- Documented that `energy_system()` owns the GameOver transition when energy reaches zero / `ENERGY_DEPLETED_EPSILON`.
- Updated `rhythm-spec.md` to remove instant-death MISS language.
- Updated `energy-bar.md` to agree with rhythm-spec and runtime behavior.
- Documented cleanup/offscreen MISS semantics and the current fixed-step order caveat.
- Added focused death-model coverage for MISS drain, deferred GameOver, timing recovery, Burnout Dead-zone behavior, depleted-energy clamping, and cleanup/offscreen misses.

## Coordinator tightening

- Found and fixed an adjacent Energy Bar doc drift that still said energy was modified in "two places only" even though cleanup/offscreen misses now drain energy.
- Added cleanup/offscreen MISS coverage to `tests/test_death_model_unified.cpp`.
- Reconfigured CMake so the new test source was included by the `tests/*.cpp` glob.

## Validation

- `git diff --check` on #82 docs passed.
- New death-model test file whitespace check passed.
- Native build passed for `shapeshifter_tests`.
- Focused tests passed: 71 assertions in 27 test cases.
- Full native non-benchmark suite passed: 2561 assertions in 772 test cases.

## Review

Kujan approved. Non-blocking note: a future explicit `TimingTier::Bad` energy-drain test would further document the Bad-timing path.

## GitHub

Comment posted to issue #82.
