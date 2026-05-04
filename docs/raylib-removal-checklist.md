# Raylib Final-Removal Checklist (Phase 7 artifact)

> Preparation artifact only. **Do not execute removal in this slice.**

## Preconditions

- [ ] SDL2 backend reaches feature parity for gameplay, UI, input, timing, and audio.
- [ ] Native + web CI lanes pass with `-DSHAPESHIFTER_BACKEND=sdl2`.
- [ ] Release owner signs off that raylib fallback is no longer required.

## Code removal plan

- [ ] Remove raylib-specific backend implementations from `app/platform/*/*_raylib.cpp` where superseded.
- [ ] Remove `SHAPESHIFTER_BACKEND_RAYLIB` compile-path conditionals after SDL2 becomes default-only.
- [ ] Remove raylib-only tests/fixtures that no longer apply.
- [ ] Clean stale migration shims/comments that only exist for dual-backend support.

## Build system + dependency cleanup

- [ ] Remove raylib dependency wiring from `CMakeLists.txt` and `vcpkg.json` when safe.
- [ ] Delete raylib-specific CI/install setup no longer needed.
- [ ] Re-verify macOS/Linux/Windows link settings after raylib removal.

## Documentation + ops cleanup

- [ ] Update `README.md` backend section to SDL2-only instructions.
- [ ] Archive migration runbook and mark issue #372 complete.
- [ ] Record final removal commit + rollback plan in `.squad/decisions.md`.
