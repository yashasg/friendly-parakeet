# Steamworks Integration: Engine Path Decision
**Date:** 2026-05-03 | **Author:** Kobayashi (CI/CD Release Engineer) | **Status:** RECOMMENDATION

---

## Question
Is Steamworks integration materially easier with SDL2 than raylib for this codebase?

**Answer: NO.** Steamworks SDK is orthogonal to graphics/input library choice. Switching engines for this reason is **scope creep**.

---

## Technical Analysis

### What Steamworks Actually Needs
- **Win32 window handle (HWND)** for overlay hooks → raylib provides this via RGFW backend
- **Async SDK initialization** → ECS system can handle this
- **Dependency linking** → Valve provides CMake-friendly SDK
- **Platform packaging** → SteamPipe build system (independent of engine)

**Steamworks does NOT care about:**
- Whether you use polling (raylib) vs event-driven (SDL2) input
- Graphics API (OpenGL, Direct3D)
- Which windowing library you use (as long as you expose HWND/NSWindow)

### Engine Dependency Audit

| Component | Raylib | SDL2 | Steamworks Requirement |
|-----------|--------|------|------------------------|
| Input polling | ✅ Polling | Event-driven | Either works |
| Window handle | ✅ RGFW backend | ✅ Native API | **Both work** |
| Overlay integration | ✅ Possible | ✅ Possible | Window hooks (platform-specific) |
| Threading/callbacks | ✅ Abstraction layer | Abstraction layer | Steamworks runs in separate thread |

### Risk: Switch to SDL2

**RISK LEVEL: MEDIUM-HIGH**

- **Input system:** Complete rewrite (`input_system.cpp` + `InputState` component)
- **Rendering:** raylib abstracts OpenGL; SDL2 is lower-level → all draw calls change
- **Platform coverage:** SDL2 iOS support is incomplete; breaks current web/iOS builds
- **Timeline:** 6-12 weeks of integration + regression testing
- **Breakage:** Existing gameplay tuning, touch input, audio integration all destabilized

### Risk: Keep Raylib + Integrate Steamworks

**RISK LEVEL: LOW**

- **Isolation:** Steamworks wraps in platform-agnostic layer (separate from ECS)
- **Existing examples:** Valve provides C++ bindings + sample code
- **Impact radius:** Zero changes to input/render systems
- **Timeline:** 2-4 weeks for basic integration (achievements, overlay, cloud saves)
- **Validation:** Existing test suite remains valid

---

## Pragmatic Recommendation (5 bullets)

1. **Keep raylib.** Steamworks SDK is engine-agnostic; switching is not justified.
2. **Create `SteamworksSystem`** as an isolated ECS system that wraps the Valve SDK (no dependencies on rendering/input).
3. **Tackle Steamworks in platform/release layer:** Build scripts, AppID config, SteamPipe depot setup — this is the real work.
4. **Defer iOS/web:** Focus Steamworks on Windows + Mac desktop builds first; mobile remains vanilla.
5. **Risk-benefit:** ~2-4 week effort vs. 6-12 month engine rewrite with 3-6 month gameplay regression cycle.

---

## Dependency Summary

| Axis | Engine Choice | Platform/Release Pipeline |
|------|---------------|----|
| Affects raylib → SDL2 switch | ✗ Decoupled | |
| Affects Steamworks integration | ✗ No | ✓ **Central** |
| Overlay support | ✓ Both work | Requires HWND/NSWindow (OS-level, not engine-level) |
| Achievements/stats | ✓ Both work | SDK linking + ECS wrapper system |
| Packaging | ✓ Both work | SteamPipe manifests (independent of engine) |

---

## Decision
**RECOMMENDATION: Proceed with raylib + Steamworks integration.**

Next steps: Prototype SteamworksSystem wrapper; schedule platform/release pipeline work (SteamPipe, AppID, build integration).
