---
name: "audio-backend-single-source"
description: "Keep runtime audio ownership in one module"
domain: "audio"
confidence: "high"
source: "audit-derived"
---

## Pattern

### Single-source audio runtime ownership

When a project has both engine-compat wrappers and platform backends, keep device lifecycle, music handle ownership, and playback-time bookkeeping in one runtime module. Other layers should forward to that module instead of re-implementing `Mix_*` orchestration or time tracking.

### Migration closure gate (avoid partial-state false positives)

If an issue requires moving mutable runtime audio state into ECS context, do not declare completion when only one slice moves (for example, music clock/override). Closure requires **both**:
- music runtime timing/override state migrated to ECS-owned context, and
- audio device lifecycle/readiness state migrated off process-global statics.

## Why

- Prevents drift where one path changes pause/resume timing semantics and another does not.
- Removes duplicate static/global state and pointer ownership risks.
- Makes ECS/runtime boundary explicit: systems mutate ECS context; runtime module owns backend handles.

## Apply It Here

- Consolidate duplicated logic currently split between `app/audio/music_backend.cpp` and `app/runtime/runtime_compat.cpp`.
- Keep one authoritative implementation for init/shutdown, load/play/pause/resume/stop, and elapsed music time.
- Keep wrappers thin and behaviorally identical across call paths.

## Migration checklist (ECS-owned runtime state)

When static globals already exist in backend/runtime audio code, migrate in this sequence:

1. Introduce ECS context structs for:
   - audio device lifecycle flags (init/mixer/open state),
   - music runtime clock state (playing/paused/start/pause accounting),
   - deterministic test override state (optional).
2. Thread these structs through backend APIs as references; do not read/write hidden static locals.
3. Update gameplay/session callsites to use the ECS-owned structs (`game_loop`, session setup, playback system).
4. Migrate tests to set/clear override state via ECS context instead of process-global toggles.
5. Remove old static singleton state only after all callsites are switched.

## Anti-Patterns

- Marking runtime-state migration “done” when `MusicContext` is migrated but backend device state is still held in function-static globals.
