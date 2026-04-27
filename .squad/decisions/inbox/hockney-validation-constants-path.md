# Decision: beat_map_loader validation constants path resolution

**Author:** Hockney  
**Date:** 2026-05  
**Issue:** #289

## Decision

`load_validation_constants(app_dir)` in `beat_map_loader.h/.cpp` now uses the project's standard dual-path pattern:
1. `app_dir + "content/constants.json"` (app-directory-first, for bundled platforms)
2. `"content/constants.json"` (CWD-relative fallback)

## Rationale

Meyer's singleton was removed because it locked constants at first-use and only tried CWD, breaking on iOS/WASM-style bundle layouts. The dual-path pattern is already established in `play_session.cpp` lines 43-54.

## Impact for other agents

- Callers with `GetApplicationDirectory()` (e.g. any future game system calling `validate_beat_map`) should call `load_validation_constants(GetApplicationDirectory())` and pass the result to `validate_beat_map(map, errors, vc)`.
- The 2-arg `validate_beat_map(map, errors)` stays for test / non-raylib contexts; it uses CWD only.
- `ValidationConstants` struct is now public in `beat_map_loader.h` — can be stored in the ECS registry context if needed by other systems.
