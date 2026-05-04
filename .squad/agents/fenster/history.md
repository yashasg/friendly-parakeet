# Fenster — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Tools Engineer
- **Joined:** 2026-04-26T02:07:46.545Z

## Learnings

Core learnings from audio/timing/UI migrations:
- Audio backend ownership must have one source of truth: keep SDL_mixer device/music lifecycle + playback timing in single runtime module, then make wrappers thin forwarders to avoid drift.
- Runtime init policy should be explicit and strict: return init success/failure from `game_loop_init`, gate `game_loop_run`/`game_loop_shutdown` on result, make shutdown resilient to partially initialized registries.
- GuiLabel font size is global, not per-control; use `GuiSetStyle(DEFAULT, TEXT_SIZE, N)` for screen-specific sizing.
- When migrating UI systems, watch for layered rendering where old+new execute sequentially; check for both data dependencies AND visual render functions.
- Screen controllers can mix generated (static) + custom (dynamic) rendering; generated layout for chrome, controller adds dynamic content via raylib/raygui primitives.
- Migration cleanup can hide regression scope; always grep for screen-specific draw/render functions.

## 2026-05-04 — Phase 5 Audio + Timing Migration Slice (SDL2 abstraction-first)

Implemented first Phase 5 abstraction slice to decouple timing/music from direct raylib while keeping behavior parity:

- Added `platform::audio::music_backend` wrapper API for music device lifecycle + stream operations.
- Migrated `song_playback_system`, `play_session`, `game_loop` shutdown, and `sfx_bank` audio-device checks to wrapper.
- Added `platform::timing::clock` with SDL2-backed monotonic time and deterministic test override hooks.
- Migrated gameplay HUD critical-pulse fallback to `platform::timing::now_seconds()`.
- Added non-flaky invariants tests: `test_music_backend.cpp` and `test_platform_timing_clock.cpp`.

Validation: Both raylib and SDL2 backends build/test passing; full suite blocked by pre-existing branch issues unrelated to this slice.

## 2026-05-04 — Phase 5 completion: SDL2-native music backend + sync validation

Completed remaining Phase 5 scope:

- Implemented SDL2-native music backend path: audio device init/shutdown, decoded wave load, queue-based play/pause/resume/stop, playback-time reporting, repeat handling.
- Preserved raylib as fallback/default backend with compile-time backend routing.
- Added deterministic music-time override hooks for non-flaky end-to-end sync tests.
- Expanded song playback and music backend tests for authoritative audio clock usage and restart/sync invariants.

Validation: Both backends pass all audio/timing/song/system tagged tests.

## 2026-05-04T10:56:32Z — Scribe: Team spawn manifest completion

Scribe orchestrated team spawn completion. Your audit findings have been merged to decisions.md.

## 2026-05-04T11:08:09Z — Re-Audit: Audio/Runtime Enforcement Priorities

**Session:** Fenster + Kujan parallel re-audits on SDL2 migration audio stack.

**Your finding (2026-05-04T11:08:09Z):** Audio/runtime enforcement priorities identified:
- ✓ Duplicate SDL_mixer orchestration reduced at wrapper layer (`app/audio/music_backend.cpp` now forwards).
- ✗ Authoritative lifecycle/timing still lives in `app/runtime/runtime_compat.cpp` — remains high-risk runtime singleton surface.
- ✗ Runtime init policy not strict enough: `game_loop_init` can early-return on failure without init status contract; `main.cpp` unconditionally runs/shuts down.

**Remediation priority (next implementation):**
1. Explicit init success contract (fail-fast on init failure).
2. Partial-init-safe shutdown guards.
3. Continued wrapper thinning so only one module owns audio state transitions.

**Related:** Kujan audit confirms same SDL_mixer consolidation progress; wrappers/static state/collision duplication remain open blockers.

**See:** `.squad/agents/fenster/history-archive.md` for UI/rendering/testing work (2026-05-20, 2026-04-29, 2026-04-30).
