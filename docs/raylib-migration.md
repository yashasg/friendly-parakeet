# Historical Note: superseded migration plan

This document is retained only as historical context.

## Current architecture direction (authoritative)

- Use direct **SDL2 / SDL_mixer / SDL_ttf** and **glm** APIs at runtime boundaries.
- Keep ECS data backend-neutral: components stay plain data with no rendering/audio backend handles.
- Do **not** introduce wrapper abstraction layers (`Renderer`, `AudioEngine`, `InputHandler`, `core_types`, `runtime_compat`, etc.).

The old SDL2→raylib plan in this file is no longer the active direction.
When migration work is planned, use this decision as the source of truth.
