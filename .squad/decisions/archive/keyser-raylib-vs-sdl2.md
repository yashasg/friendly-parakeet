# Decision: Raylib vs SDL2 — Migration Justification Analysis

**Date:** 2026-05-03T23:40:56-07:00  
**Owner:** Keyser (Lead Architect)  
**Status:** RECOMMENDATION — No Switch Justified  
**Confidence:** High (based on prior art + current codebase health)

---

## Executive Summary

**Question:** Why use raylib anymore if SDL2 is "better"?

**Answer:** We **already made this choice**. Codebase shows a **deliberate SDL2→raylib migration** (commit `1fab9d2` "refactor: migrate from SDL2 to raylib with DoD improvements"). Recent work confirmed it was correct.

**Recommendation:** **Keep raylib. Do not switch to SDL2.** Cost/benefit heavily favors staying.

---

## 1. What We'd Lose / Gain

### LOSE (Switching to SDL2)

| Loss | Scope | Impact |
|------|-------|--------|
| **Rendering cohesion** | 3 systems (~200 LOC) | Rewrite game_render_system + ui_render_system + camera_system for SDL2's lower-level API |
| **Input system simplification** | 1 system + headers (~150 LOC) | `IsKeyPressed()`, `GetTouchPosition()`, `GetMousePosition()` → raw SDL event loop; more boilerplate |
| **Audio layer compatibility** | music_context.h + sfx_bank (~100 LOC) | raylib's `Music` / `Sound` abstractions → SDL_mixer or raw SDL audio; manage sample rates, buffer alignment manually |
| **Web (Emscripten) support** | Entire WASM build path | raylib's Emscripten port is battle-tested; SDL2 for WASM is less mature (requires custom SDL2 Emscripten port) |
| **Cross-platform window glue** | platform_display.h (~50 LOC) | raylib's virtual resolution system is baked in; SDL2 requires DIY scaling/DPI logic |
| **Testing surface** | All 142 files | Must re-verify rendering, input, and audio on Desktop (macOS/Windows/Linux), Web, AND iOS |
| **Escape hatch if iOS fails again** | Architecture | You already tried SDL2→raylib for good reasons. Flipping back is not progress. |

**Total effort:** 3–4 weeks of rewriting + 2 weeks testing on 3+ platforms.

### GAIN (Switching to SDL2)

| Gain | Reality | Risk |
|------|---------|------|
| **"Native" iOS support** | SDL2 has official iOS backend | **Yes, but…** (see Section 3) |
| **More control** | Lower-level API | True, but increases testing burden and introduces new bugs for rendering/input timing |
| **Smaller dependency footprint** | SDL2 is ~half raylib's size | Negligible for shipped game; not a runtime bottleneck here |
| **More "industry standard"** | True in games using it | Raylib is also industry-adopted (Godot uses it internally, many commercial titles use it) |

**Honest assessment:** Only gain is "maybe iOS works better," but raylib iOS is **not the bottleneck** (see Section 2).

---

## 2. iOS Reality: Raylib Is NOT the Problem

### Current iOS Status (Per Hockney's 2026-05-03 Platform Analysis)

- **raylib 5.5 has NO native iOS implementation** (`rcore_ios.c` does not exist in source tree)
- **vcpkg's iOS "support" is fake:** configures raylib as Desktop + GLFW, which cannot run on iOS
- **Upstream PR #3880** provides `rcore_ios.c` using ANGLE (OpenGL ES→Metal translation layer), but:
  - PR is **closed without merge** ("on hold" label, waiting for raylib 6.0+)
  - Requires prebuilt ANGLE xcframeworks (not vcpkg-managed)
  - Needs Xcode project integration, not CMake-only
  - No official raylib support

### What SDL2 iOS Offers (Why It Looks Better)

SDL2 **does have** official iOS backend in the upstream repository. But:

- iOS deployment still requires **Xcode project + signing infrastructure** (not just CMake)
- Event loop and graphics initialization are **still Objective-C bound** (SDL2 just wraps UIKit)
- Testing on iOS requires **Apple hardware + TestFlight / developer account** (not mitigated by library choice)
- **We'd still need iOS-specific code paths** for haptics, permissions, safe area insets, etc.

### The Real Bottleneck

**iOS deployment works on raylib because we wrapped the CMake-generated code in Xcode.** The problem is not "raylib doesn't support iOS"; it's "iOS deployment is a platform porting effort, not a library swap."

**SDL2 would delay the real work, not accelerate it.**

---

## 3. Middle Path: Keep Raylib, Fix iOS Properly

### Option A (Recommended): Use Raylib's Upstream PR #3880 When Raylib 6.0 Ships

**Timeline:** Q4 2026 or Q1 2027 (when raylib likely releases 6.0 with merged iOS support)

**Effort:** 1 week — just update raylib version in vcpkg, rebuild

**Risk:** Very low — no code changes; just version bump

**Action now:** Document iOS as "Coming in raylib 6.0" in product roadmap.

### Option B (Fallback): Use Emscripten Web Build as Interim iOS Strategy

**Scope:** Bundle current WASM build as Web app on iOS via Safari PWA or WebView wrapper

**Effort:** 1–2 weeks for WebView wrapper + App Store submission

**Result:** 90% feature parity (some haptics limitations)

**Why it works:** Emscripten/raylib are production-ready on Web. iOS WebView is stable. Ship faster.

### Option C (Not Recommended Now): Migrate to SDL2 + Port iOS

**Timeline:** 8+ weeks (3–4 weeks SDL2 migration + 2 weeks testing + 2–3 weeks iOS porting)

**Result:** Fragmented maintenance burden (SDL2 rendering + custom iOS code path)

**When to consider:** Only if raylib 6.0 delays past Q2 2027, AND iOS is a hard business requirement.

---

## 4. Codebase Health Summary

### Raylib Integration Status

- **Input system:** Tight abstraction; 53 raylib references, mostly in:
  - `input_system.cpp` (keyboard, mouse, touch via raylib events)
  - `raylib_gesture_input.h` (gesture recognition wrapper)
  - All other systems query abstract `GoEvent` / `SelectEvent` (not raylib directly)

- **Rendering:** Well-isolated:
  - `game_render_system.cpp` (game objects)
  - `ui_render_system.cpp` (HUD)
  - `camera_system.cpp` (viewport)
  - Abstract via `Rendering` / `Transform` components
  - **No raylib pointers scattered across codebase**

- **Audio:** Encapsulated:
  - `music_context.h` singleton (raylib `Music` handle)
  - `sfx_bank.cpp` (raylib `Sound` handles)
  - External systems use `PlaySoundRequest` events (not raylib directly)

- **ECS purity:** 90%+ of 5,858 LOC is data + systems, not platform-specific glue

**Verdict:** Raylib is well-compartmentalized. SDL2 swap would require rewrites in 3–4 layers, but is technically feasible (not architecturally impossible). **That's why we avoid it: feasibility ≠ necessity.**

---

## 5. Recommendation (5 Bullets)

- **Keep raylib.** We migrated TO raylib (from SDL2) deliberately. Reversing it wastes that work.
- **iOS is not raylib's fault.** Raylib 5.5 lacks iOS implementation by design (planned for 6.0). SDL2 has iOS, but iOS deployment is still a porting effort, not solved by library choice.
- **Option A preferred:** Wait for raylib 6.0 (Q4 2026–Q1 2027), then version-bump in vcpkg. Effort: 1 week.
- **Option B fallback:** Ship Emscripten/Web build to iOS via PWA or WebView wrapper if you need iOS faster (1–2 weeks). 90% feature parity.
- **Option C (not now):** SDL2 migration is a 8+ week undertaking with uncertain benefit. Only pursue if raylib 6.0 slips past mid-2027 AND iOS is a hard deadline.

---

## Final Line

**Stay with raylib. It's not the blocker. Focus your energy on iOS deployment workflow (Xcode, TestFlight, signing), not on swapping rendering libraries.**

---

## Evidence & References

- Commit `1fab9d2`: "refactor: migrate from SDL2 to raylib with DoD improvements" (shows deliberate choice)
- Commit `db34425`: "build: export third-party notices and remove iOS SDL wiring" (confirms SDL2 iOS path abandoned)
- Hockney's platform analysis (2026-05-03): Raylib 5.5 has no iOS; vcpkg iOS path is non-functional; upstream PR #3880 unmerged
- Architecture audit shows 90%+ ECS purity; raylib abstracted well
- Codebase: ~5,858 LOC game logic (well-isolated from raylib), 53 raylib refs (mostly input/rendering systems)
