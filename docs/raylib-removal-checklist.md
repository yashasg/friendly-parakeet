# Raylib Final-Removal Checklist (Phase 7 closure)

## Preconditions

- [x] SDL2 backend reached parity for migrated window/input/render/timing/audio surfaces.
- [x] Native + web CI lanes pass with `-DSHAPESHIFTER_BACKEND=sdl2`.
- [x] Fallback backend deprecation approved for execution.

## Code removal plan

- [x] Remove raylib backend implementations from `app/platform/*/*_raylib.cpp`.
- [x] Remove `SHAPESHIFTER_BACKEND_RAYLIB` compile-path conditionals and selection wiring.
- [x] Remove stale migration shims/comments tied to dual-backend selection.
- [ ] Remove all remaining raylib API usage from non-backend runtime utilities.

## Build system + dependency cleanup

- [x] Remove raylib backend selection from CMake/build scripts.
- [x] Delete raylib-specific CI backend validation lanes.
- [x] Re-verify macOS/Linux/Windows SDL2-only link paths.
- [ ] Drop raylib package dependency from `CMakeLists.txt` and `vcpkg.json` (blocked by remaining direct API usage in shared runtime modules).

## Documentation + ops cleanup

- [x] Update `README.md` backend section to SDL2-only commands.
- [x] Update migration runbook to post-migration SDL2-only state.
- [ ] Record final rollback plan in `.squad/decisions.md`.
