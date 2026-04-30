---
name: "content-root-consolidation"
description: "Consolidate game asset roots to a single content/ tree across runtime, build, and docs."
domain: "build-packaging"
confidence: "high"
source: "earned"
tools:
  - name: "rg"
    description: "Find all path references to legacy roots."
    when: "Before and after migration to verify no stale references remain."
  - name: "apply_patch"
    description: "Apply coordinated path updates across CMake/runtime/docs."
    when: "When changing path literals and comments in tracked files."
---

## Context
Use this when the repo has diverging content roots (for example `assets/` and `content/`) and runtime/build/WASM/docs are out of sync. The goal is one canonical shipped root so native copies, browser preload, and loader fallbacks all agree.

## Patterns
- Move retained files into `content/` first (for example fonts into `content/fonts/`), then delete the old root directory.
- Update runtime path literals (`GetApplicationDirectory()` fallback arrays) to use the new root.
- Update CMake `file(GLOB ...)`, post-build copy destinations, and Emscripten `--preload-file` flags in one pass.
- Sweep docs and authoring prompts that output file paths (`assets/beatmaps/...`) so generated artifacts land under `content/`.
- Validate with repo-wide search for `assets/` and directory existence checks (`assets/` must not exist).

## Examples
- Runtime font fallback: `content/fonts/LiberationMono-Regular.ttf`
- WASM preload: `--preload-file ${CMAKE_SOURCE_DIR}/content@/content`
- Beatmap authoring target: `content/beatmaps/<song_id>.json`

## Anti-Patterns
- Keeping dual roots alive with partial redirects.
- Updating docs only, without runtime/build path changes.
- Leaving stale Emscripten preload entries for removed directories.
