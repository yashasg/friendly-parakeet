# Historical Note: completed/superseded migration plan

This document is retained only as historical context.

## Current architecture direction (authoritative)

- The shipped runtime uses direct **raylib/raygui** APIs at windowing, input,
  rendering, UI, and audio boundaries.
- **glm** is used for math data and transforms.
- ECS gameplay code remains data-oriented: components are plain data and
  systems are free functions.
- Do **not** introduce wrapper abstraction layers (`Renderer`, `AudioEngine`,
  `InputHandler`, `core_types`, `runtime_compat`, etc.).

The old SDL2 -> raylib plan in this file is no longer an active migration plan;
the repository has already moved to raylib/raygui. For current dependency
status, use `docs/ongoing_migration.md` and the build files as the source of
truth.
