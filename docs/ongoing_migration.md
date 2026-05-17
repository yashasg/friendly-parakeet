# Migration Documentation Status

This file is the authoritative index for migration-related documentation.

## Current architecture direction

1. Runtime/platform calls use direct **raylib / raygui**.
2. ECS components and gameplay systems remain plain data + free functions.
3. No compatibility wrapper layer should be introduced.

## Historical migration docs

- `docs/raylib-migration.md` is historical. The migration to raylib/raygui is complete; keep it only as context for why wrapper layers were rejected.
- `docs/sokol-migration.md` is historical. It describes an abandoned SDL2 to Sokol plan and is not implementation guidance.

## Repository implications

- New runtime work should extend the existing raylib/raygui path unless a new architecture decision supersedes this file.
- Runtime-only systems must stay separated from the headless ECS API surface.
- Backend handle types must not leak into common ECS component headers.

If any code or docs conflict with this decision, treat this file as authoritative.
