# Baer Decision: ECS Regression Test Coverage (#336 + #342)

**Filed:** 2026-04-28  
**Author:** Baer (Test Engineer)  
**Issues:** #336 (miss_detection idempotency), #342 (on_construct signal lifecycle ungated)

## Decision

Added two new non-platform-gated test files to cover P2 and P3 gaps identified in the April 27 ECS audit:

### `tests/test_miss_detection_regression.cpp` (issue #336)

- Tag: `[miss_detection][regression][issue336]`
- 5 test cases, 28 assertions
- Covers: N expired obstacles → N MissTag + ScoredTag; idempotence (second call no-ops); above-DESTROY_Y not tagged; already-ScoredTag excluded; non-Playing phase no-op
- Verified: both syntax check (against dirty working tree) and object compilation pass with zero warnings

### `tests/test_signal_lifecycle_nogated.cpp` (issue #342)

- Tag: `[signal][lifecycle][issue342]`
- 6 test cases, 23 assertions  
- Covers: on_construct<ObstacleTag> increments counter; on_destroy<ObstacleTag> decrements; wire_obstacle_counter idempotent (no double-connect); counter reaches zero; no underflow; wired flag blocks re-entry
- No PLATFORM_DESKTOP guard — runs on Linux/macOS/Windows CI

## Build Context

Full `shapeshifter_tests` binary cannot link due to pre-existing in-progress breakage in `benchmarks/bench_systems.cpp` (uses deleted `EventQueue` and old `ButtonPressEvent` constructor — another agent's refactor mid-flight).

Both new files:
- Pass `-fsyntax-only` check against dirty working tree
- Compile to object files cleanly: zero errors, zero warnings  
- CMake build.make lists both in the shapeshifter_tests target (GLOB picked them up after reconfigure)

The pre-existing binary at `build/shapeshifter_tests` (built April 27, 812 tests, 2749 assertions) confirms baseline is green. New tests not in that binary.

## Impact

- #336 acceptance criteria fully met by code. Will execute on next clean build after other-agent refactor lands.
- #342 acceptance criteria fully met by code. Signal path (on_construct<ObstacleTag>) now covered without PLATFORM_DESKTOP gate.

## Recommendation

Do not close #336 or #342 until `bench_systems.cpp` is updated and the test binary successfully links with both new test files included. Both issues are code-complete and closure-ready once the in-progress EventQueue→dispatcher refactor is finished.
