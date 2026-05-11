# Runtime Dependency Status

This file tracks the active dependency-boundary direction and supersedes older
backend migration notes.

## Decision

1. The shipped runtime uses direct **raylib/raygui** APIs for windowing, input,
   rendering, UI, and audio.
2. **glm** is the math type dependency for transforms and geometry data.
3. ECS components and gameplay systems remain plain data + free functions.
4. No compatibility wrapper layer should be introduced.

## Repository implications

- There is no active SDL, Sokol, or backend-wrapper migration plan.
- `docs/raylib-migration.md` and `docs/sokol-migration.md` are historical only.
- Runtime-only systems must stay separated from the headless ECS API surface.
- Backend handles should stay at runtime/render/audio boundaries unless a
  component is intentionally runtime-facing.

If any code or docs conflict with this decision, treat this file as authoritative.
