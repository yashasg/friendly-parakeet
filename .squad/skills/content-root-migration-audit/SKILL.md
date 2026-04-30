---
name: "content-root-migration-audit"
description: "Audit and validate removal of legacy assets/ roots in favor of a single content/ root."
domain: "testing"
confidence: "high"
source: "observed"
tools:
  - name: "rg"
    description: "Fast string/path sweep across code, docs, and workflows."
    when: "Finding stale path literals like assets/ after migration."
  - name: "bash"
    description: "Runs build/test/path checks."
    when: "Verifying migration safety and checking for leftover directories."
---

## Context
Use this when a repo is consolidating resource roots (for example, deleting `assets/` and keeping `content/` only). The risk is partial migrations where build/runtime/editor surfaces disagree on root paths.

## Patterns
- Sweep literals with `rg "assets/|\\bassets\\b"` across source, CMake, workflows, docs, tests, and tools.
- Sweep filesystem state with `find . -type d -name '*assets*'` to confirm removed directories.
- Separate **stale path references** from **intentional generic wording** (e.g., “game assets”) and platform terms (e.g., `Assets.xcassets`).
- Validate with existing project commands (build + non-bench C++ tests + editor JS checks/tests).

## Examples
- Runtime/build migration paths: `CMakeLists.txt`, `app/ui/text_renderer.cpp`, `content/fonts/`.
- Validation commands:
  - `cmake --build build -- -j4`
  - `./build/shapeshifter_tests "~[bench]"`
  - `node --check tools/beatmap-editor/js/*.js`
  - `node --test tools/beatmap-editor/test/*.test.js`

## Anti-Patterns
- Declaring migration complete from code grep alone without checking live directories.
- Treating all `assets` words as blockers (many are generic prose or Apple bundle terminology).
- Skipping editor-side checks when path constants are shared with tooling.
