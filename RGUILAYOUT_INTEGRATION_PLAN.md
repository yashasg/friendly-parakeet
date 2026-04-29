# rguilayout Integration Plan

> Historical planning artifact. Current committed source of truth is:
> - `content/ui/screens/*.rgl` (authoring)
> - `app/ui/generated/*_layout.h` (generated embeddable headers)
> - `app/ui/screen_controllers/` (runtime behavior)
> Standalone rguilayout exports are scratch-only and must not be committed.

**Status:** Architecture spine defined, ready for implementation  
**Author:** Keyser (Lead Architect)  
**Date:** 2025-04-28

## Summary

Phase 2 complete: all screens have `.rgl` and generated `.c/.h` files. But generated files are standalone programs that can't compile into the game. This defines the integration path forward.

## Solution: Custom Template + Adapter Layer

**Integration flow:**
```
.rgl files → (custom template) → header-only .h → C++ adapters → ui_render_system
```

**File structure:**
```
app/ui/
├── rgl_generated/          # Generated layout headers (NO main(), NO RAYGUI_IMPLEMENTATION)
│   ├── title_layout.h
│   ├── tutorial_layout.h
│   └── ... (8 screens)
├── rgl_adapters/           # Hand-written C++ adapters (added incrementally)
│   ├── title_adapter.cpp
│   └── ...
└── [delete old *.c/*.h]    # Remove standalone generated files
```

## Why This Works

✅ **No ECS layout mirrors** — adapters call functions, don't copy data  
✅ **Generated files own layout** — no duplication into project structs  
✅ **Incremental** — migrate screen-by-screen, JSON stays until ready  
✅ **Build-safe** — header-only avoids ODR, one raygui implementation  
✅ **No CMake changes** — adapters auto-globbed by existing `app/ui/*.cpp` pattern

## Migration Strategy

**Screen by screen:**

1. Re-export one screen (e.g., title) using custom template → `rgl_generated/title_layout.h`
2. Write `rgl_adapters/title_adapter.cpp` that calls the layout function
3. Wire adapter into `ui_render_system.cpp` with feature flag
4. Test behavior parity
5. Remove JSON dependency for that screen
6. Repeat for next screen

**Parallel paths during migration:**
- Migrated screens: call rgl adapter
- Unmigrated screens: use existing JSON system
- No big-bang cutover

## Next Steps

### 1. Create Custom Template (TOOLING TASK)

Create `tools/rguilayout/templates/shapeshifter_header.rgl` that exports:

- ✅ Layout drawing functions (inline or extern "C")
- ✅ Named control return values (button pressed states)
- ✅ Rectangle exports for dynamic content slots
- ❌ NO `main()` function
- ❌ NO `RAYGUI_IMPLEMENTATION` define
- ❌ NO window/loop scaffolding

**Example desired output:**
```c
// app/ui/rgl_generated/title_layout.h
#pragma once
#include "raylib.h"
#include "raygui.h"  // NO IMPLEMENTATION define

static inline int DrawTitleLayout(void) {
    int result = 0;
    GuiLabel((Rectangle){ 160, 480, 400, 48 }, "SHAPESHIFTER");
    if (GuiButton((Rectangle){ 260, 1050, 200, 50 }, "EXIT")) result |= 0x2;
    return result;
}
```

### 2. Add raygui Implementation

Current state: NO `RAYGUI_IMPLEMENTATION` exists in game code (only in standalone generated files).

**Action:** Add exactly one implementation site, likely:
- New file: `app/ui/raygui_impl.cpp` with `#define RAYGUI_IMPLEMENTATION` + `#include "raygui.h"`
- Or: add to existing `app/ui/text_renderer.cpp` if it already uses raygui

### 3. Proof of Concept (Title Screen)

1. Re-export `content/ui/screens/title.rgl` with custom template
2. Write `app/ui/rgl_adapters/title_adapter.cpp`:
   ```cpp
   #include "rgl_generated/title_layout.h"
   #include "../components/dispatcher.h"
   
   void render_title_screen(entt::registry& reg) {
       int result = DrawTitleLayout();
       #ifdef PLATFORM_DESKTOP
       if (result & 0x2) { /* handle exit */ }
       #endif
   }
   ```
3. Wire into `ui_render_system.cpp` with `#ifdef USE_RGL_TITLE` guard
4. Test: behavior matches current JSON-driven title screen
5. Remove title JSON dependency

### 4. Iterate

Repeat for remaining 7 screens: tutorial, level_select, gameplay, paused, game_over, song_complete, settings.

### 5. Final Cleanup

After all screens migrated:
- Delete `ui_loader.h`, `ui_layout_cache.h`
- Delete `content/ui/screens/*.json`
- Remove feature flags from `ui_render_system.cpp`

## Open Issues

1. **Custom template authoring** — requires understanding rguilayout's template variable syntax. See `tools/rguilayout/USAGE.md` section "Code Generation" for `$(GUILAYOUT_DRAWING_C)` etc.

2. **Dynamic text binding** — scores, song names, stats need runtime values. Template must export:
   - Named Rectangle slots for dynamic content areas
   - Or: function parameters for dynamic strings

3. **Adapter API signature** — should all adapters use same signature `void render_X_screen(entt::registry&)` or per-screen custom signatures?

## Impact

**Files deleted:** `app/ui/*.c`, `app/ui/*.h` (8 standalone generated files)  
**Files added:** `app/ui/rgl_generated/*.h` (8 header-only), `app/ui/rgl_adapters/*.cpp` (incremental)  
**Files changed:** `ui_render_system.cpp` (add adapter calls)  
**Build changes:** None (adapters auto-globbed)  
**Future delete:** JSON layout system after all screens migrated

## Constraints Preserved

This architecture respects all prior design decisions:

- Layout data stays in generated files (never mirrored into ECS/ctx)
- No `RaguiAnchors<ScreenTag>` or layout POD duplication
- No UI widget entities for static menus
- Incremental migration with parallel runtime paths
- Build-safe (no ODR violations, no duplicate raygui implementations)

## References

- **Full decision:** `.squad/decisions/inbox/keyser-rguilayout-integration-spine.md`
- **Spec:** `design-docs/raygui-rguilayout-ui-spec.md`
- **Tool docs:** `tools/rguilayout/USAGE.md`
- **Current build:** `CMakeLists.txt:100` — `file(GLOB UI_SOURCES app/ui/*.cpp)`

---

**Handoff:** Template creation and first proof-of-concept (title screen) can proceed independently. Architecture spine is stable.
