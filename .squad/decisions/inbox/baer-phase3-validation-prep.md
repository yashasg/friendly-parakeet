# Baer — Phase 3 Rendering Validation Prep (SDL2)

**Date:** 2026-05-04T00:32:05.528-07:00  
**Agent:** Baer (Test Engineer)  
**Scope:** Pre-stage stable validation assets for SDL2 Phase 3 rendering parity work.

## Decision

Adopt deterministic, non-snapshot validation for SDL2 render-path bring-up:

1. Add renderer-level command counters + frame-time override hooks in the SDL2 backend.
2. Add focused tests that verify hook wiring and deterministic behavior.
3. Add a concise manual parity checklist for cross-platform (macOS/Linux/Windows) visual validation.

## Rationale

- Pixel snapshots are brittle across GPU drivers/platforms and would create maintenance debt during active migration.
- Command-path counters provide stable backend wiring checks without needing a live GL capture pipeline.
- Frame-time override enables deterministic perf-sanity assertions for backend timing code paths.
- Manual checklist closes the remaining gap (final visual confirmation) while automation covers structural contracts.

## Artifacts

- `app/platform/graphics/renderer.h`
- `app/platform/graphics/renderer_sdl2.cpp`
- `tests/test_renderer_sdl2_validation.cpp`
- `docs/sdl2-phase3-rendering-parity-checklist.md`

## Ready-to-use outcomes for Keaton

- Immediate backend harness for SDL2 render command-path validation.
- Deterministic timing hook for upcoming render-loop parity checks.
- Cross-platform manual verification list to run at Phase 3 completion.
