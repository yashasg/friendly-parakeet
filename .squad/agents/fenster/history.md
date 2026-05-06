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
- Fresh post-migration #374 check: `MusicContext` now owns mutable music clock + override fields in ECS (`app/audio/music_context.h`), but audio device lifecycle flags are still static globals in `app/runtime/runtime_compat.cpp` (`RuntimeAudioState`), so migration is partial.
- Persistent architecture hotspot paths for direct-engine/no-wrapper criteria: `app/runtime/runtime_types.h`, `app/runtime/runtime_api.h`, `app/runtime/runtime_compat.cpp`, and `app/rendering/renderer_backend.h`.
- Ongoing dedupe/EnTT cleanup priority remains `app/systems/collision_system.cpp` (graded/ungraded branch duplication and `ctx().find` + `emplace` fallback for `SongState` in hot path).

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

## 2026-05-04T11:29:38Z — Audio audit pass for Issue #374 objective

Ran a fresh audio-focused audit over `app/` and `tests/` plus rebuild/test validation (`cmake -B build -S . -Wno-dev`, `cmake --build build`, `./build/shapeshifter_tests "[audio]"`, `./build/shapeshifter_tests "[song_playback][audio][timing]"`; all passed).

Findings:
- Issue #374 objective remains open: mutable runtime audio state still lives in static globals (`RuntimeMusicState`, `RuntimeAudioState`) inside `app/runtime/runtime_compat.cpp`.
- Additional mutable static remains in wrapper layer: `MusicTimeOverride` singleton in `app/audio/music_backend.cpp`.
- ECS currently carries only partial music runtime state (`MusicContext` stream/loaded/started/volume), not backend timing/device lifecycle state.

Blueprint direction recorded for team: move backend-mutated music clock + audio device lifecycle state into registry context components and thread those through audio backend API/callsites, then remove static singleton state from runtime compatibility layer.

## 2026-05-04T11:29:38Z — Scribe Session: Decision Merge + Audit Orchestration

**Cross-agent update:** Fenster audio audit decisions merged into decisions.md (1 inbox file). Orchestration log created.

**Team-relevant outcomes:**
- Audio-specific audit completed; Issue #374 objective still open.
- Identified mutable static-backed `RuntimeMusicState` and `RuntimeAudioState` in runtime layer; `MusicTimeOverride` in audio layer.
- ECS `MusicContext` incomplete; only stores handle/flags, not backend clock/device state.
- Duplicate SDL_mixer orchestration creates drift risk between wrapper and runtime.

**Team direction:** Add ECS context structs for audio device lifecycle + music runtime clock; change backend API to accept registry-owned state references; update callsites to route through ECS-owned runtime state.

**Next phase:** Implementation agent will thread runtime-state references through backend API.

## 2026-05-04 — Scribe: Audio audit outcomes logged
- Audit gate gate REJECT: Issue #374 remains OPEN
- Audio device runtime state must move into ECS context (next priority)
- Orchestration logs written; decisions.md merged

## 2026-05-04T11:55:54Z — Kujan post-audio criteria audit complete

Kujan completed full post-audio architecture audit against criteria (no-wrappers, SOLID, dedupe, EnTT principles).

**Top findings (audit pass: 2 blockers identified for next cycle):**
1. Issue #373, #374, #375 confirmed CLOSED and verified in code
2. Blocker 1 (P0): Virtual renderer wrapper violates architecture contract
   - Files: `app/rendering/renderer_backend.h`, `app/rendering/renderer_backend_sdl2.cpp`, `app/systems/game_render_system.cpp`, `app/game_loop.cpp`
   - Patch: Remove `class Renderer` ABC; replace with free functions in `platform::graphics` namespace
3. Blocker 2 (P1): Collision system lazy ECS ctx init + dedupe failure  
   - File: `app/systems/collision_system.cpp:41-43` + 126–258
   - Patch: Move `SongState` emplace to `game_loop_init`; unify `can_grade_shape` branches

**Team-relevant:** Renderer blocker owns broader callsite surface (routes to rendering/platform specialist). Collision blocker self-contained (any implementation agent). Both can execute in single pass.

**Status:** Findings merged to decisions.md; orchestration logged. Ready for implementation handoff.

## 2026-05-05T17:24:56.671-07:00 — SDL_mixer direct audio runtime migration

- Replaced raylib music runtime calls in `app/systems/audio_runtime.cpp` with direct SDL_mixer API (`Mix_LoadMUS`, `Mix_PlayMusic`, `Mix_HaltMusic`, `Mix_FreeMusic`, `Mix_PauseMusic`, `Mix_ResumeMusic`, `Mix_VolumeMusic`).
- Added `MusicContext::repeat` in `app/components/audio.h` so loop behavior remains data-owned and explicit for SDL_mixer playback.
- Confirmed no raylib `Music`/`*MusicStream` calls remain in `audio_runtime.cpp`; build is currently blocked by unrelated rendering/runtime include fallout.
## 2026-05-06: SDL/glm cross-agent migration team

**Orchestration Log:** .squad/orchestration-log/2026-05-06T00-38-50Z-fenster.md
**Session Log:** .squad/log/2026-05-06T00-38-50Z-direct-sdl-rewire.md

Collaborated on rendering/audio decoupling. All team work merged to decisions.md (2026-05-06).
