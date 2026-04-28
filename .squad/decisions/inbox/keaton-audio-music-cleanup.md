# Decision: Audio/Music Types Rehomed from components/ to systems/

**Date:** 2026-04-28  
**Author:** Keaton  
**Status:** IMPLEMENTED

## Decision

`app/components/audio.h` and `app/components/music.h` have been deleted. Their types now live in:
- `app/systems/audio_types.h` — SFX enum, AudioQueue, audio_push/audio_clear, SFXPlaybackBackend, SFXBank
- `app/systems/music_context.h` — MusicContext

## Rationale

- `app/components/` is for ECS entity components (data attached to entities). `AudioQueue`, `SFXBank`, `SFXPlaybackBackend`, and `MusicContext` are context singletons managed by audio systems — not entity components.
- raylib `Sound` and `Music` are already used directly inside these types; no wrapper indirection is added.
- The move makes the dependency direction explicit: systems own their types.

## Impact

All includes updated across app/, tests/, and benchmarks/. Build: zero warnings. Tests: 2854 assertions pass.
