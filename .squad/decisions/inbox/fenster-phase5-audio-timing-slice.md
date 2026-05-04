# Fenster Decision — Phase 5 audio/timing first slice

Date: 2026-05-04
Branch: feature/sdl2-migration-phase-1-abstraction-layer
Issue context: #372 (SDL2 migration)

## Decision
Implement Phase 5 incrementally by introducing platform wrappers first, then swapping concrete backend behavior behind those wrappers.

## Applied in this slice
1. **Music abstraction layer**: `app/platform/audio/music_backend.{h,cpp}` wraps music device and stream operations.
2. **Timing abstraction layer**: `app/platform/timing/clock.{h,cpp}` provides monotonic clock with SDL2-backed source on SDL2 builds.
3. **Behavior-parity migration**:
   - `song_playback_system` and `play_session` now call abstraction APIs (not direct raylib music funcs).
   - HUD fallback pulse time now uses platform timing API.
   - SFX bank audio readiness checks go through platform audio readiness API.
4. **Safety/fallback posture**:
   - Current concrete audio behavior remains raylib-compatible through the abstraction so existing music-sync logic remains stable while SDL2-native audio implementation is still pending.

## Why
- Keeps mergeable scope small and testable.
- Preserves existing sync semantics (`song_playback_system` authoritative music clock path) while making backend swap feasible.
- Avoids destabilizing current runtime during Phase 3/4 parallel work.

## Follow-ups for full Phase 5
- Introduce SDL2-native decoded music playback path behind `music_backend`.
- Expand backend parity tests to include pause/resume/restart semantics under SDL2-native path.
- Route additional direct raylib audio touchpoints (if any remain) into platform audio.
