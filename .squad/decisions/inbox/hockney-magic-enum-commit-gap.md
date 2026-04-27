# Decision: magic_enum Build Wiring Must Co-Land With Include Usage

**Author:** Hockney  
**Date:** 2026-05  
**Status:** RESOLVED — commit `72c83b5`

## Context

Kujan rejected the combined batch due to commit `8fbce9c` introducing `#include <magic_enum/magic_enum.hpp>` across the codebase while the corresponding `vcpkg.json` and `CMakeLists.txt` wiring was left as working-tree-only changes. Fresh checkout of HEAD could not build.

## Decision

Created follow-up commit `72c83b5` (branch `user/yashasg/ecs_refactor`) containing only:

| File | Change |
|------|--------|
| `vcpkg.json` | `"magic-enum"` added to dependencies |
| `CMakeLists.txt` | `find_package(magic_enum CONFIG REQUIRED)`, SYSTEM deps loop entry, PUBLIC link on `shapeshifter_lib` |

Did **not** amend existing commits (reviewer protocol).

## Team Rule Established

Any commit that introduces `#include` headers from a new third-party library must simultaneously commit:
1. `vcpkg.json` — package name in dependencies array
2. `CMakeLists.txt` — `find_package`, SYSTEM deps loop, `target_link_libraries`

Leaving dependency wiring as working-tree-only while committing include usage creates a broken HEAD that fails fresh checkout. This is a build-integrity regression regardless of whether local artifacts mask it.

## Validated

`cmake -Wno-dev -B build -S .` on HEAD `72c83b5` → `Configuring done (0.7s)`, exit 0.
