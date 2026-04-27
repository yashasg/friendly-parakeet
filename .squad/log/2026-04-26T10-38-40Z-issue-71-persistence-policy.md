# Issue #71: Specify persistence backend per platform

**Timestamp:** 2026-04-26T10:38:40Z  
**Squad:** Ralph (implementation), Coordinator (tightening/validation), Kujan (review), Scribe (logging)

## Scope

Resolved the persistence policy ambiguity for TestFlight/App Store and CI/dev platforms.

## Outcome

- `design-docs/architecture.md` now defines the persistence architecture:
  - iOS/TestFlight/App Store v1 uses JSON persistence through the shared C++ filesystem abstraction and must resolve inside the app sandbox.
  - Desktop CI/dev uses `%APPDATA%/shapeshifter` on Windows, `~/.shapeshifter` on macOS/Linux, and `.` fallback.
  - WASM CI/dev uses the current-directory Emscripten filesystem path; durable browser storage/IDBFS is future work if web becomes customer-facing.
- `design-docs/game.md` now separates implemented persisted data from v1-required FTUE persistence.
- `design-docs/game-flow.md` now specifies FTUE persistence requirements without claiming FTUE runtime serialization exists today.
- Current persisted data is documented as high scores and settings only.
- FTUE run count is documented as a v1 requirement, not a current implementation.

## Coordinator tightening

- Removed iCloud sync and durable IDBFS overclaims.
- Corrected settings save behavior to match code: saved when settings controls change.
- Corrected high-score save behavior to match code: saved on GameOver/SongComplete only when score improves.
- Corrected the high-score JSON example after Kujan found the missing top-level `scores` wrapper.

## Validation

- `git diff --check -- README.md design-docs/game.md design-docs/game-flow.md design-docs/architecture.md design-docs/feature-specs.md`
- `cmake -B <verify-dir> -S . ...`
- `cmake --build <verify-dir> --target shapeshifter_tests`
- `<verify-dir>/shapeshifter_tests "[settings],[high_score]"`
- `<verify-dir>/shapeshifter_tests "~[bench]"`

## Review

Kujan requested one correction to the high-score JSON example, then approved on re-review.

## GitHub

Comment posted to issue #71.
