# Component Boundary Cleanup — Test Plan

**Issue:** User-reported cleanup of misnamed/misplaced headers:
- `app/components/shape_vertices.h` — not a component, should use raylib Draw functions
- `app/components/settings.h` — not a component, just audio settings data, should move to audio.h or music.h

**Current State (discovered):**
- ✅ `app/components/settings.h` ALREADY MOVED to `app/util/settings.h` (commits ea2c979, fdcd709, pending git add)
- ⏳ `app/components/shape_vertices.h` STILL EXISTS, needs evaluation for removal/relocation

**Test Engineer:** Baer  
**Date:** 2026-04-27  
**Baseline:** 845 test cases, 2854 assertions, all pass

---

## §1 — Regression Surface

### 1.1 `shape_vertices.h` Removal

**Current usage:**
- `app/systems/game_render_system.cpp` — uses `shape_verts::CIRCLE`, `shape_verts::CIRCLE_SEGMENTS` for floor ring rendering
- `tests/test_perspective.cpp` — 12 tests covering circle/hex/square/triangle vertex tables, CCW winding, unit magnitude, symmetry
- `benchmarks/bench_perspective.cpp` — likely benchmarks vertex table lookups

**Impact:**
- The vertex tables are used **only** for floor ring rendering (draw_floor_rings)
- Raylib has `DrawCircle()`, `DrawRectangle()`, `DrawTriangle()`, `DrawPoly()` functions
- Current floor ring implementation uses manual triangle fan rendering with `rlBegin(RL_TRIANGLES)` and sub-sampling from CIRCLE table
- **Risk:** Floor ring rendering may need custom vertex math for inner/outer radius ring segments
- **Verdict:** If raylib Draw functions don't support annulus (ring) rendering with inner/outer radius, Keaton will need to inline the vertex math or use a different approach

**Affected tests:**
- `test_perspective.cpp` — 12 `[shape_verts]` tests will become obsolete if vertex tables are removed
- Floor ring rendering has regression test at line 218-244 (sub-segment coverage)
- No existing visual regression tests for floor appearance

### 1.2 `settings.h` Move

**STATUS: ALREADY COMPLETED** ✅

**Git history:**
- Commits ea2c979, fdcd709 — refactor to move settings out of components/
- `app/components/settings.h` deleted (git rm pending)
- `app/util/settings.h` now contains SettingsState and SettingsPersistence structs
- All 12 consumers updated to `#include "util/settings.h"` (0 references to components/settings.h remain)

**Validated:**
- Full test suite passes (845 cases, 2854 assertions)
- Zero stale includes to old path
- No further action needed for settings cleanup

**User's original request:**
- "move it into audio.h or music.h" — **REJECTED** (already moved to correct location: util/settings.h)
- **Rationale:** SettingsState contains haptics_enabled, reduce_motion, ftue_run_count — not just audio data. Util location is architecturally correct.

---

## §2 — Validation Strategy

### 2.1 Build Portability

**Commands:**
```bash
# Clean build
rm -rf build
cmake -B build -S . -Wno-dev
cmake --build build --target shapeshifter_tests

# Zero-warning check (critical)
cmake --build build 2>&1 | grep -i warning | grep -v vcpkg | grep -v "duplicate libraries"
```

**Expected:** Zero warnings from project code (only vcpkg/third-party notes allowed)

**Platform coverage:**
- macOS ARM64 (primary dev platform) ✓
- Linux CI (not tested — GitHub Actions placeholders)
- WASM build (not tested — emscripten build excludes tests entirely)

### 2.2 Core Test Suites

**Baseline validation:**
```bash
./build/shapeshifter_tests "~[bench]" --reporter compact
```

**Expected:** All 845 test cases pass, 2854 assertions

**Focus areas after shape_vertices.h removal:**

#### a) Rendering (if shape_vertices.h removed)
```bash
./build/shapeshifter_tests "[shape_verts]" --reporter compact
./build/shapeshifter_tests "[floor]" --reporter compact
./build/shapeshifter_tests "[camera3d]" --reporter compact
```

**Risk:** No visual regression tests for floor ring rendering. Manual smoke test required:
```bash
./build/shapeshifter
# Visual check: floor rings appear as annulus (donut) shapes in lane 0
```

#### b) Settings (already validated — baseline established)
```bash
./build/shapeshifter_tests "[settings]" --reporter compact
./build/shapeshifter_tests "[ecs]" --reporter compact
./build/shapeshifter_tests "[haptic]" --reporter compact
./build/shapeshifter_tests "[ui_source]" --reporter compact
./build/shapeshifter_tests "[ftue]" --reporter compact
```
**Status:** ✅ All pass in current state (settings.h move already complete)

### 2.3 Include Dependency Chain

**Validation (shape_vertices.h only):**
```bash
# Find all files that include shape_vertices.h
grep -r "#include.*shape_vertices\.h" app/ tests/ --include="*.h" --include="*.cpp"

# After Keaton's changes, verify no stale includes remain
grep -r "components/shape_vertices\.h" app/ tests/ --include="*.h" --include="*.cpp"
```

**Expected:** All include paths removed or updated to new location (if moved instead of inlined)

### 2.4 Stale Include Check (settings.h — follow-up)

**Check for any stragglers:**
```bash
grep -r "components/settings\.h" app/ tests/ --include="*.h" --include="*.cpp"
# Expected: no matches (already moved to util/settings.h)
```

**Status:** ✅ Validated — zero references to old path remain

---

## §3 — Risk Assessment

### High Risk (P0 — must validate)

1. **Floor ring rendering broken** — if shape_verts::CIRCLE removed without replacement, floor rings will not render or crash
   - **Mitigation:** Visual smoke test + floor ring coverage test at test_perspective.cpp:218
   - **Owner:** Keaton must provide alternative (raylib API or inline math)

2. **Zero-warning policy violated** — any unused includes, undefined symbols, or type mismatches after shape_vertices.h removal
   - **Mitigation:** `grep -i warning` in build output
   - **Owner:** Baer validates before merge

### Medium Risk (P1 — should validate)

3. **Benchmark build broken** — bench_perspective.cpp may not compile if shape_vertices.h removed
   - **Mitigation:** Benchmark target not in CI; defer to future cleanup if needed
   - **Owner:** Keaton decides during implementation

4. **shape_verts tests obsolete** — 12 tests in test_perspective.cpp no longer relevant if vertex tables removed
   - **Mitigation:** Keaton should remove tests if vertex tables removed; keep Camera3D tests (lines 1-77)
   - **Owner:** Keaton decides during implementation

### Low Risk (P2 — informational)

5. **Settings.h already moved** ✅ — no further action needed
   - **Status:** Completed in commits ea2c979, fdcd709
   - **Validation:** Full test suite passes, zero stale includes

---

## §4 — Test Commands Summary

**Pre-implementation baseline:**
```bash
cmake -B build -S . -Wno-dev && cmake --build build --target shapeshifter_tests
./build/shapeshifter_tests "~[bench]" --reporter compact
# Expected: 845 test cases, 2854 assertions, all pass ✅ VALIDATED
```

**Post-implementation validation (shape_vertices.h only):**
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
# Expected: all tests pass (count may differ if shape_verts tests removed)

# Focused regression (if shape_vertices.h removed)
./build/shapeshifter_tests "[camera3d]" --reporter compact
# Expected: Camera3D tests still pass; shape_verts tests removed or adapted

# Smoke test (manual)
./build/shapeshifter
# Visual check: floor rings render correctly in lane 0
```

**Stale include check:**
```bash
grep -r "components/shape_vertices\.h" app/ tests/ --include="*.h" --include="*.cpp"
# Expected: no matches (removed or relocated)

grep -r "components/settings\.h" app/ tests/ --include="*.h" --include="*.cpp"
# Expected: no matches (already validated — zero stale includes) ✅
```

---

## §5 — Decision for Implementation

**Recommendation:**

### shape_vertices.h
- **Action:** Defer to Keaton's judgment
- **Rationale:** Raylib's `DrawCircle()` and friends don't support annulus (ring) rendering with inner/outer radius. Current implementation uses manual triangle fan with sub-sampled circle vertices. Keaton must either:
  1. Inline the vertex math into game_render_system.cpp (remove header entirely)
  2. Keep the header but rename to `util/geometry.h` or similar (not a component)
  3. Switch to a different rendering approach (e.g., DrawCircleSector with thickness)

### settings.h
- **Action:** ALREADY DONE — moved to `app/util/settings.h` in commits ea2c979, fdcd709
- **Status:** ✅ COMPLETED (git rm pending for old file)
- **User's suggestion:** "move to audio.h or music.h" — **REJECTED** by previous implementer (Keaton/McManus)
- **Correct decision:** util/settings.h is architecturally appropriate for global user preferences (audio offset, haptics, FTUE progress)

**User's suggestion is half-correct:** shape_vertices.h is indeed not a component and could be removed/inlined. But settings.h was ALREADY correctly moved to util/ (not audio.h, which would be incorrect).

---

## §6 — Test Coverage Gaps (out of scope for this cleanup)

**Identified but not blocking:**

1. **No visual regression tests** — floor ring appearance, shape rendering, UI layout all untested
2. **WASM build has zero tests** — CMakeLists.txt:350 excludes shapeshifter_tests on EMSCRIPTEN
3. **Platform-gated tests invisible on CI** — test_test_player_system.cpp 12 tests skip on non-DESKTOP builds
4. **Input system untested** — swipe threshold, multi-touch, zone routing not exercised
5. **Real beatmaps never loaded in tests** — content/beatmaps/*.json ignored by CI

---

## §7 — Deliverables

**By Baer (this task):**
- ✅ This test plan document
- ✅ Baseline test run (845 cases, 2854 assertions, all pass)
- ✅ Validated: settings.h already moved to util/ in commits ea2c979, fdcd709
- ⏳ Post-implementation validation (after Keaton's shape_vertices.h work)
- ⏳ Decision memo to .squad/decisions/inbox/ (after validation)

**By Keaton (implementation):**
- ⏳ Remove or relocate shape_vertices.h (with replacement rendering approach)
- ⏳ Update all include paths in production and test code
- ⏳ Remove obsolete tests (if vertex tables removed)
- ⏳ Zero-warning build on macOS ARM64

**By Coordinator (approval gate):**
- ⏳ Kujan code review (structural correctness, include hygiene)
- ⏳ Baer validation sign-off (zero-warning build, all tests pass)

**Note:** settings.h cleanup is COMPLETE — only shape_vertices.h work remains.

---

**End of Test Plan**
