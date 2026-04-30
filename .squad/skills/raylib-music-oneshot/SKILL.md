# Skill: Raylib Music One-Shot Safety

## Use when

- A gameplay flow depends on deterministic song end (results screen, phase transition, cleanup).
- Music should stop at track end instead of repeating.

## Pattern

Use a project-local loader seam that takes repeat intent:

`Music stream = load_music_stream(path, /*repeat=*/false);`

and keep direct `stream.looping = ...` writes out of gameplay/session orchestration code.

## Why

Do not rely on backend/default looping behavior. Explicit one-shot mode prevents hidden repeat loops that can block end-of-song transitions.

## Applied in this repo

- `app/audio/music_stream.h` (`load_music_stream(const char* path, bool repeat)`).
- `app/session/play_session.cpp` in `setup_play_session(...)`.
