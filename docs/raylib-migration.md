# Historical Note: completed raylib migration

This document is retained only as historical context.

## Current architecture direction (authoritative)

- Use direct **raylib / raygui** APIs at runtime boundaries.
- Keep ECS data backend-neutral: components stay plain data with no rendering/audio backend handles.
- Do **not** introduce wrapper abstraction layers (`Renderer`, `AudioEngine`, `InputHandler`, `core_types`, `runtime_compat`, etc.).

The old SDL2 to raylib migration is complete; do not use this file as an active task list.
For current status, use `docs/ongoing_migration.md` as the source of truth.
