# Issue #76: FTUE/tutorial is specified but not implemented

**Timestamp:** 2026-04-26T21:50:49Z  
**Squad:** Ralph (implementation), Coordinator (validation), Kujan (review), Scribe (logging)

## Scope

Implemented the user-approved minimal TestFlight FTUE path rather than cutting tutorial onboarding from scope.

## Outcome

- `SettingsState` now owns `ftue_run_count`, persisted in `settings.json`.
- `ftue_run_count == 0` means the minimal tutorial is incomplete.
- `ftue_run_count >= 1` means the minimal tutorial is complete and Level Select is unlocked directly from Title.
- Existing settings files without `ftue_run_count` load successfully and default to incomplete.
- Title Confirm routes to Tutorial for first-time players and Level Select for completed players.
- Tutorial Confirm marks FTUE complete, saves settings, clears queued input, and transitions to Level Select.
- Added `GamePhase::Tutorial`, `ActiveScreen::Tutorial`, tutorial UI routing, tutorial button hitbox, and `content/ui/screens/tutorial.json`.
- Automated test-player sessions auto-confirm Tutorial so they continue into gameplay.
- Docs now state TestFlight v1 ships a minimal first-run FTUE gate; the aspirational 5-run tutorial remains future expansion.

## Coordinator validation

- Reconfigured CMake to ensure the new tutorial screen is included in copied UI assets.
- Confirmed `content/ui/routes.json` and `content/ui/screens/tutorial.json` parse as valid JSON.
- Confirmed built output contains `content/ui/screens/tutorial.json`.
- Ran focused FTUE/settings/game-state/UI tests.
- Ran the full native non-benchmark suite.

## Validation

- `git diff --check` on #76 relevant files passed.
- Native CMake build passed for `shapeshifter` and `shapeshifter_tests`.
- Focused tests passed: 471 assertions in 135 test cases.
- Full native non-benchmark suite passed: 2548 assertions in 771 test cases.

## Review

Kujan reviewed the FTUE persistence, routing, UI, docs, switch handling, and tests. No significant issues were found.

## GitHub

Comment posted to issue #76.
