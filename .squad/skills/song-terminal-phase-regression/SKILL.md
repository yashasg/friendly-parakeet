# SKILL: Song-End Terminal Phase Regression

**Domain:** C++ ECS gameplay/state testing  
**Applicable to:** Bugs where end-of-song UI/phase does not appear, or playback appears to restart/loop near terminal transitions.

---

## Pattern

When song completion depends on multiple systems, test the contract across ticks instead of system-by-system in isolation.

### Deterministic contract

1. Start in `GamePhase::Playing` with `SongState` just before `duration_sec`.
2. Run `song_playback_system` once to cross duration.
3. Assert `song.finished == true` and `song.playing == false`.
4. Run `game_state_system` once: assert `transition_pending == true` and `next_phase == SongComplete`.
5. Run `game_state_system` again: assert `phase == SongComplete`.
6. Run `song_playback_system` again in terminal phase: assert no restart (`playing` stays false, `finished` stays true, song time no longer advances).

---

## Why this catches real regressions

- Guards the full system boundary (`song_playback_system` → `game_state_system`) where many end-screen bugs hide.
- Captures the two-tick nature of transition processing (`transition_pending` then execution).
- Prevents accidental "unlatch" regressions that re-enable playback after finish.

---

## Testability seam to request when needed

If production uses `GetMusicTimePlayed()` directly, headless tests cannot force end-of-track wrap reliably.
Add an injectable music-clock provider (or wrapper function) so tests can simulate wrap-around and pin non-looping behavior at real stream boundaries.

---

## Repeat-policy refactor check (source-level gate)

For changes that move loop control into a loader helper, run two mandatory source sweeps:

1. `music->stream.looping = false` should have **zero** production callsites in play-session setup.
2. Raw `LoadMusicStream(` in session setup should be replaced by a project-local helper that takes explicit repeat policy (`false` for play-session).

If the helper seam is missing, do not fabricate brittle audio-device tests; rely on terminal-phase regression tags plus source-gate evidence and report the seam gap.
