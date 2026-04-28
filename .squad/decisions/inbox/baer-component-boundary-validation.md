---
date: 2026-04-28
author: baer
status: informational
issue: component-boundary-cleanup
---

# Component Boundary Cleanup — Validation Report

## Context

User reported two headers misplaced in `app/components/`:
1. `shape_vertices.h` — "not a component, raylib has Draw<shape> functions"
2. `settings.h` — "not a component, just audio data, move to audio.h or music.h"

## Findings

### settings.h — ✅ ALREADY DONE

**Status:** Moved to `app/util/settings.h` in commits ea2c979, fdcd709 (refactor #286, #318).

**Current state:**
- `app/components/settings.h` deleted (git rm pending)
- `app/util/settings.h` contains SettingsState + SettingsPersistence structs
- 12 consumers updated to new include path
- Zero stale includes to old path
- Full test suite passes (845 cases, 2854 assertions)

**User's suggestion rejected:**
- "move to audio.h or music.h" — architecturally incorrect
- SettingsState contains: audio_offset_ms, haptics_enabled, reduce_motion, ftue_run_count
- This is a **global user preferences singleton**, not audio-specific state
- Util location is correct

### shape_vertices.h — ⏳ AWAITS IMPLEMENTATION

**Status:** Still exists at `app/components/shape_vertices.h` (correct — cleanup incomplete).

**Contents:** Constexpr vertex tables for Circle (24), Hexagon (6), Square (4), Triangle (3).

**Usage:**
- `app/systems/game_render_system.cpp` — floor ring rendering with inner/outer radius (draw_floor_rings, lines 100-130)
- `tests/test_perspective.cpp` — 12 tests covering vertex properties
- `benchmarks/bench_perspective.cpp` — vertex table benchmarks

**Problem with user's suggestion:**
- Raylib has `DrawCircle()`, `DrawRectangle()`, `DrawTriangle()`, `DrawPoly()`
- BUT none support **annulus (ring) rendering** with inner/outer radius
- Current implementation uses manual triangle fan with `rlBegin(RL_TRIANGLES)` and sub-sampled circle vertices
- Cannot directly replace with raylib API

**Options:**
1. **Inline vertex math** into game_render_system.cpp (remove header entirely)
2. **Rename/move** to `app/util/geometry.h` (not a component, but preserve as reusable data)
3. **Keep existing** — header is stable, render logic works, low coupling

**Recommendation:** Option 2 (move to util/geometry.h) — preserves reusability, fixes naming, minimal risk.

## Regression Surface

**P0 High risk:**
- Floor ring rendering broken if CIRCLE table removed without replacement
- Zero-warning policy violated (unused includes, missing symbols)

**P1 Medium risk:**
- Benchmark build broken (bench_perspective.cpp depends on shape_vertices.h)
- 12 shape_verts tests obsolete (test_perspective.cpp lines 80-244)

**P2 Low risk:**
- No visual regression tests for floor appearance (manual smoke test required)

## Validation Commands

**Post-implementation (after shape_vertices.h work):**
```bash
# Clean rebuild
rm -rf build
cmake -B build -S . -Wno-dev
cmake --build build --target shapeshifter_tests 2>&1 | tee build_output.txt

# Zero-warning check
grep -i warning build_output.txt | grep -v vcpkg | grep -v "duplicate libraries"
# Expected: no output

# Full test suite
./build/shapeshifter_tests "~[bench]" --reporter compact
# Expected: all tests pass

# Stale include check
grep -r "components/shape_vertices\.h" app/ tests/ --include="*.h" --include="*.cpp"
# Expected: no matches

# Manual smoke test
./build/shapeshifter
# Visual check: floor rings render as annulus (donut) in lane 0
```

## Decision

**For squad:**
- ✅ settings.h cleanup complete — no further action needed
- ⏳ shape_vertices.h requires implementation decision (Keaton or user)
- ❌ User's audio.h suggestion rejected for settings (wrong architectural boundary)

**For user/implementer:**
- If removing shape_vertices.h: must provide alternative rendering approach (raylib API insufficient)
- If relocating: `app/util/geometry.h` is recommended location
- If keeping: document why it's not a component (it's data tables, not state)

**Baseline validated:** 845 test cases, 2854 assertions, all pass (macOS ARM64).

**Test plan:** See `COMPONENT_CLEANUP_TEST_PLAN.md` for full validation strategy.
