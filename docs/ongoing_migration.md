# Ongoing Migration Status

This file tracks the active dependency-boundary direction.

## Decision

1. Runtime/platform calls should use direct **SDL2 / SDL_mixer / SDL_ttf** and **glm**.
2. ECS components and gameplay systems remain plain data + free functions.
3. No compatibility wrapper layer should be introduced.

## Repository implications

- Previous raylib migration docs are historical only.
- Runtime-only systems must stay separated from the headless ECS API surface.
- Backend handle types must not leak into common ECS component headers.

If any code or docs conflict with this decision, treat this file as authoritative.
