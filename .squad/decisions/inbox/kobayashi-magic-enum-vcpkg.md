# Decision: magic_enum via vcpkg — Single Source of Truth

**Author:** Kobayashi  
**Date:** 2026-05  
**Status:** IMPLEMENTED

## Decision

`magic_enum` (header-only enum reflection library) is wired through vcpkg as the canonical dependency, not vendored.

## Changes

| File | Change |
|------|--------|
| `vcpkg.json` | Added `"magic-enum"` to `dependencies` array (alphabetical, between `entt` and `raylib`) |
| `CMakeLists.txt` | Added `find_package(magic_enum CONFIG REQUIRED)` after `find_package(EnTT ...)` |
| `CMakeLists.txt` | Added `magic_enum::magic_enum` to `_system_deps` SYSTEM-include loop |
| `CMakeLists.txt` | Linked `magic_enum::magic_enum` PUBLIC from `shapeshifter_lib` (propagates to all consumers: exe and tests) |

## Rationale

- Public link on `shapeshifter_lib` means both `shapeshifter` and `shapeshifter_tests` inherit the target without repeating it.
- SYSTEM-include suppression prevents magic_enum headers from generating `-Werror` failures (header-only, but still guarded for consistency with all other third-party deps).
- vcpkg-managed: version tracked in lockfile, consistent across all 4 CI platforms (Linux, macOS, Windows, WASM).

## Validated

- `cmake -Wno-dev -B build -S .` → vcpkg installed `magic-enum:arm64-osx@0.9.7#1`, configure succeeded.
- `cmake --build build --target shapeshifter_lib` → `[100%] Built target shapeshifter_lib`, zero warnings.
