# Issue #74: Specify pause/resume behavior for audio-driven gameplay

**Timestamp:** 2026-04-26T11:23:19Z  
**Squad:** Ralph (implementation), Coordinator (tightening/validation), Kujan (review), Scribe (logging)

## Scope

Defined and enforced the pause/resume contract for song-driven gameplay: music, `song_time`, beat scheduling, gameplay simulation, queued input, and app focus/interruption expectations.

## Outcome

- `MusicContext` now tracks whether `PauseMusicStream()` is active.
- `song_playback_system()` pauses music on `GamePhase::Paused`, resumes exactly once after explicit resume, and stops music when leaving the song context.
- `SongState.song_time`, current beat computation, and beat scheduling freeze outside `GamePhase::Playing`.
- Resume is explicit through paused-active Confirm/Resume; there is no countdown and no artificial grace period.
- `EventQueue` is cleared when entering pause and after paused UI handling so stale gameplay input cannot fire on resume.
- Focus loss while `Playing` transitions to `Paused`; focus regain does not auto-resume.
- Pause docs now include iOS TestFlight background/interruption QA and desktop/WASM focus-loss proxy cases.

## Coordinator tightening

- Found that cleanup/miss processing, lifetime timers, and particle gravity could still advance while paused.
- Added paused guards to `cleanup_system()`, `lifetime_system()`, and `particle_system()`.
- Added regression tests for paused cleanup, paused lifetime timers, paused particle gravity, and epsilon-based cleanup energy clamping.
- Updated architecture documentation so cleanup/effect systems are described as pause-guarded rather than always draining during pause.

## Validation

- `git diff --check` on #74 relevant files passed.
- Native CMake build passed for `shapeshifter` and `shapeshifter_tests`.
- Focused tests passed: 319 assertions in 130 test cases.
- Full native non-benchmark suite passed: 2511 assertions in 766 test cases.

## Review

Kujan requested one change: cleanup miss energy clamping should use `ENERGY_DEPLETED_EPSILON` consistently. The coordinator fixed it, added regression coverage, revalidated, and Kujan approved on re-review.

## GitHub

Comment posted to issue #74.
