# Decision: audio/music component header relocation

**Date:** 2026-04-28  
**Reviewer:** Kujan  
**Author:** Keaton  
**Verdict:** APPROVED

## Decision

`app/components/audio.h` and `app/components/music.h` are removed. Their game-specific types (`SFX` enum, `AudioQueue`, `SFXPlaybackBackend`, `SFXBank`, `MusicContext`) are relocated to `app/systems/audio_types.h` and `app/systems/music_context.h` respectively.

`app/components/settings.h` is co-relocated to `app/util/settings.h` (semantically correct: settings are persistence-tier, not pure ECS components).

## Rationale

- These are not pure raylib wrappers — they contain game-specific queue/state types that USE raylib types.
- Placement in `app/systems/` is consistent with the pre-existing pattern of `BeatMapError` / `ValidationConstants` in `beat_map_loader.h`.
- All include sites updated; no stale references remain.
- No behavior change; pure relocation.

## Non-blocking notes

- `audio_push` / `audio_clear` inline helpers in a header file remain a style gap (decisions.md F4). Pre-existing — tracked separately.
- Ring_zone deletion + obstacle_archetypes move in same working tree are pre-approved (see prior Kujan history entry 2026-04-28).
