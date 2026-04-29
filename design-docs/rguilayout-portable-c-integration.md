# rGuiLayout Portable C Integration Spec

## Status

Active — governs the integration boundary between rguilayout-generated code and game screen controllers.

## Overview

rGuiLayout is a visual editor that generates **portable C/header files** containing UI layout data and immediate-mode draw/input-dispatch code. This spec defines how the game integrates those generated files into the screen rendering pipeline.

**Key principle:** The generated `.h` file is the source of truth for UI layout and control placement. Game code consumes this portable C API; it does not copy rectangles, rebuild layout caches, or mirror widget state into ECS.

## Goal: Portable Generated C as Integration Boundary

The rguilayout export—a portable, pre-compiled `.h` file with no external dependencies except `raylib.h`—becomes the contract between:

- **Upstream (tool):** rGuiLayout authoring and code generation.
- **Downstream (game):** Screen controller integration and game state binding.

```
[rguilayout v4.0 tool]
         |
         | author in GUI, export
         v
[content/ui/screens/*.rgl] (editable source)
         |
         | generate portable C/header
         v
[app/ui/generated/*_layout.h] (portable, no dependencies)
         |
         | included by screen controller
         v
[app/ui/screen_controllers/*_screen_controller.cpp] (game-specific glue)
         |
         | calls into render system
         v
[app/systems/ui_render_system.cpp]
```

**Contract:** The generated header is **final**. It is not hand-edited, not post-processed, and not compiled separately. It is included directly by the screen controller and calls raygui primitives during the game's render frame.

## Source-of-Truth Flow

### 1. Author in rGuiLayout (Offline)

- Open `tools/rguilayout/rguilayout.app/` (vendored standalone tool).
- Load or create `content/ui/screens/<screen>.rgl` layout file.
- Arrange raygui controls, anchors, and geometry.
- Export via CLI or GUI: `rguilayout --input <screen>.rgl --output app/ui/generated/<screen>_layout.h --template embeddable_layout.h`.
- If extracting from default standalone output is needed, write scratch output under `build/rguilayout-scratch/` only; do not commit it.

### 2. Generated Portable Header

The export produces `app/ui/generated/<screen>_layout.h`:

- **Header-only** or header + minimal inline functions (no `.c` compilation unit required).
- **No external dependencies** except `raylib.h` (and implicitly raygui, which is a raylib header).
- **No `main()` or event loop** — it's a library, not a standalone executable.
- **No `#define RAYGUI_IMPLEMENTATION`** — the caller is responsible for that (see § RAYGUI_IMPLEMENTATION ownership).
- **Exposes public API:**
  - Layout constants (width, height, viewport bounds).
  - State struct (anchors, button-press flags, derived widget state).
  - `Init()` function: returns initial state.
  - `Render(state*)` function: draws controls and updates input flags every frame.
  - Optional: named accessor functions for dynamic content (e.g., `SetScoreText(state*, const char*)`) if the template supports it.

### 3. Screen Controller Integration

A thin C++ screen controller includes the generated header and calls it:

```cpp
// app/ui/screen_controllers/title_screen_controller.cpp
#include "components/game_state.h"
#include "components/input.h"
#include "screen_controllers/screen_controller_base.h"
#include <raygui.h>
#include "ui/generated/title_layout.h"

namespace {
    using TitleController = RGuiScreenController<TitleLayoutState,
                                                  &TitleLayout_Init,
                                                  &TitleLayout_Render>;
    TitleController title_controller;
} // anonymous namespace

void init_title_screen_ui() {
    title_controller.init();
}

void render_title_screen_ui(entt::registry& reg) {
    title_controller.render();

#ifndef PLATFORM_WEB
    if (title_controller.state().ExitButtonPressed) {
        reg.ctx().get<InputState>().quit_requested = true;
    }
#endif

    if (title_controller.state().SettingsButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Settings;
    }
}
```

**Key rules for screen controllers:**
- ✅ Include generated header and call public API.
- ✅ Read game state and pass it into generated calls (e.g., dynamic text, current selection).
- ✅ Translate button-press flags into game commands.
- ✅ Apply platform-specific guards (web/desktop, accessibility).
- ❌ Do NOT copy layout rectangles, anchor positions, or control state into ECS or `reg.ctx()`.
- ❌ Do NOT rebuild layout structs or caches.
- ❌ Do NOT spawn UI widget entities or hitbox entities.

### 4. Render System Call

The UI render system dispatches to the appropriate screen controller:

```cpp
// app/systems/ui_render_system.cpp
void ui_render_system(entt::registry& reg) {
    GamePhase phase = reg.ctx().get<GamePhase>();
    switch (phase) {
        case GamePhase::Title:
            render_title_screen_ui(reg);
            break;
        case GamePhase::Tutorial:
            render_tutorial_screen_ui(reg);
            break;
        // ...
    }
}
```

## Why Not Compile Generated C Directly?

rGuiLayout exports `.c` and `.h` files, and the generated `.c` includes `#define RAYGUI_IMPLEMENTATION` by default. This is fine for **standalone tools** but problematic in a game:

- **Ownership conflict:** The game's main loop owns the window, render target, and raygui state initialization. A generated file defining `RAYGUI_IMPLEMENTATION` would conflict with the game's own raygui setup.
- **Multiple definition risk:** If multiple generated files each define `RAYGUI_IMPLEMENTATION`, the linker fails (ODR violation).
- **Simplicity:** For a game with 8 screens, it's simpler to **not compile generated C files directly**. Instead, generate header-only or minimal-inline libraries that the game calls from its own render loop.

**Solution:** Use the **embeddable template** (`tools/rguilayout/templates/embeddable_layout.h`) when exporting, which omits the `main()`, event loop, and `RAYGUI_IMPLEMENTATION` guard. The game defines `RAYGUI_IMPLEMENTATION` once via CMake source-level compile definitions on a single controller translation unit.

## Why "Portable Generated C" Instead of "Screen Controllers"?

**Terminology hierarchy:**

- **Portable generated C** is the **contract** — the primary integration boundary. It is produced by rGuiLayout, is portable across projects, and is owned upstream.
- **Screen controllers** are **implementation** — game-specific glue code that consumes the generated contract and integrates it into the game's render pipeline.

**Why this distinction matters:**

1. **Upstream ownership:** rGuiLayout (the tool) produces the generated `.h` contract. The game adopts it, not the reverse.
2. **No hand-editing generated files:** Developers must regenerate from `.rgl` after layout changes; they never hand-edit the generated `.h`.
3. **Portable by design:** The generated file is a **portable library**; it does not know about ECS, game state, or screen controllers. The screen controller provides that knowledge.
4. **Reusability:** A generated layout could theoretically be used by a different game or tool; screen controllers are always game-specific.
5. **Clarity for future developers:** "Portable C" signals "do not hand-edit, regenerate from source"; "screen controller" signals "game-specific integration point."

## API Shape: Init / State / Render

Generated layouts follow a consistent immediate-mode API:

```cpp
// Compile-time constants
#define SCREEN_NAME_LAYOUT_WIDTH  720
#define SCREEN_NAME_LAYOUT_HEIGHT 1280

// State struct: holds anchors, button-press flags, derived data
typedef struct {
    Vector2 Anchor01;
    bool StartButtonPressed;
    bool SettingsButtonPressed;
} ScreenNameLayoutState;

// Initialization: returns zeroed state struct
static inline ScreenNameLayoutState ScreenNameLayout_Init(void);

// Render: draws controls, updates input flags
// Called once per frame from within a BeginDrawing/EndDrawing block
static inline void ScreenNameLayout_Render(ScreenNameLayoutState* state);

// Optional: dynamic text/value setters (depends on template)
static inline void ScreenNameLayout_SetScore(ScreenNameLayoutState* state, const char* score_text);
```

**No update/cleanup functions** unless the template introduces custom resources (e.g., `LoadTexture()`). The portable C design assumes:
- Initialization is lightweight and done once at app startup or screen entry.
- Render is called every frame and recomputes layout state from current input.
- Cleanup is not required (no persistent resources in the layout).

## Event-Consumer Design: UI Effects Outside Layout

Dynamic visual effects (screen shake, flashes, color tints) are **UI state**, not layout geometry. The screen controller or a dedicated UI effect system manages them:

```cpp
// UI effects system (separate from layout)
struct UIEffectState {
    float flash_alpha;        // from BeatFlashEvent
    float shake_offset_x;     // from ShakeEvent
    bool danger_tint;         // from EnergyLowEvent
};

// During render:
void render_title_screen_ui(entt::registry& reg) {
    UIEffectState effects = compute_effects(reg);
    
    // Apply effects before/after layout render
    if (effects.flash_alpha > 0) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color { 255, 255, 255, (unsigned char)(effects.flash_alpha * 255) });
    }
    
    // Render layout at normal position
    TitleLayout_Render(&title_state);
    
    if (effects.shake_offset_x != 0) {
        // Optional: re-render with offset, or draw overlay
    }
}
```

The layout itself is **static**; effects are **applied around it** during the render frame. This keeps the generated portable C focused and avoids coupling layout geometry to game effects.

## File Layout and Naming

```
tools/rguilayout/
  rguilayout.app/             # Vendored rGuiLayout v4.0 GUI tool
  templates/
    embeddable_layout.h       # Custom code generation template

content/ui/screens/
  title.rgl                   # Editable layout source (rGuiLayout project file)
  tutorial.rgl
  level_select.rgl
  gameplay.rgl
  paused.rgl
  game_over.rgl
  song_complete.rgl
  settings.rgl

app/ui/
  generated/
    title_layout.h            # Portable generated layout API (include only, no compile)
    tutorial_layout.h
    level_select_screen_layout.h
    gameplay_hud_layout.h
    paused_layout.h
    game_over_layout.h
    song_complete_layout.h
    settings_layout.h
  screen_controllers/
    title_screen_controller.cpp/.h      # Screen controller: bridges generated layout to game systems
    tutorial_screen_controller.cpp/.h
    level_select_screen_controller.cpp/.h
    gameplay_hud_screen_controller.cpp/.h
    paused_screen_controller.cpp/.h
    game_over_screen_controller.cpp/.h
    song_complete_screen_controller.cpp/.h
    settings_screen_controller.cpp/.h
  
```

**Naming conventions:**
- Generated files: `<screen>_layout.h` (singular "layout"), lowercase with underscores.
- Screen controllers: `<screen>_screen_controller.cpp/.h`, matching the logical name.
- rGuiLayout projects: `<screen>.rgl`, matching controller name.

**Current pattern:** All screens use `app/ui/screen_controllers/` naming. This is the implemented, active architecture.

## RAYGUI_IMPLEMENTATION Ownership

raygui is a **header-only single-file library** that compiles its implementation only if `#define RAYGUI_IMPLEMENTATION` is set in exactly one translation unit.

**Rule:** The game defines `RAYGUI_IMPLEMENTATION` once on one source file via CMake (currently `app/ui/screen_controllers/title_screen_controller.cpp`), and excludes that source from unity batching:

```cmake
set_source_files_properties(
    app/ui/screen_controllers/title_screen_controller.cpp
    PROPERTIES
        COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION
        SKIP_UNITY_BUILD_INCLUSION TRUE
)
```

**Why this matters:**
- **ODR safety:** Only one translation unit defines the raygui implementation; multiple `.cpp` files can safely `#include "raygui.h"`.
- **Consistent global state:** raygui maintains internal state (e.g., GuiSetStyle state, focus tracking). Having one source of truth ensures consistency.
- **Build isolation:** Generated layouts do not define `RAYGUI_IMPLEMENTATION` (enforced by the embeddable template). Screen controllers include both and remain warning-free.

## Strict Non-Goals

These are explicitly out of scope for the portable C integration:

1. **ECS widget mirrors:** Do NOT create `UIWidgetTag` entities, `UIRectangle` components, or `UIButtonState` in the registry to mirror generated layout data.
2. **Copying layout data:** Do NOT read generated constants (anchor positions, button rectangles) and store them in `reg.ctx()` layout caches or hand-written structs.
3. **Hand-editing generated files:** Generated `.h` files are final. If layout changes, regenerate from `.rgl`; never manually edit the export.
4. **Compiling generated `.c` files:** Do not add a CMake target to compile generated `.c` files as separate translation units. Use header-only generated code.
5. **Standalone execution:** Generated exports are libraries, not standalone tools. The embeddable template removes `main()` and the event loop.
6. **No committed standalone scratch files:** Any temporary standalone exports must live in `build/rguilayout-scratch/` (or equivalent ignored build scratch path) and must not be committed.

## Migration History: Adapter Naming → Screen Controllers (Completed)

The codebase previously used an `app/ui/adapters/` folder with `<screen>_adapter.cpp/.h` naming. This migration to `app/ui/screen_controllers/` is **complete**. All eight screens have been implemented as screen controllers. There is no `app/ui/adapters/` folder; `app/ui/screen_controllers/` is the only and current pattern.

### Incorrect Pattern (Avoid)

If you encounter code that rebuilds layout state or copies generated rectangles, this is incorrect and must be fixed:

```cpp
// ❌ WRONG: Copying layout rectangles into a cache
TitleLayout cached_layout = rebuild_layout_from_constants(reg);

// ❌ WRONG: Storing layout in ECS context
reg.ctx<TitleLayoutCache>() = cached_layout;
```

### Correct Pattern

Use the `RGuiScreenController` template from `screen_controller_base.h` and mutate game state directly via `reg.ctx()`:

```cpp
// ✅ CORRECT: Use RGuiScreenController template
using TitleController = RGuiScreenController<TitleLayoutState,
                                              &TitleLayout_Init,
                                              &TitleLayout_Render>;
TitleController title_controller;
title_controller.init();

// ✅ CORRECT: Call render, then dispatch via ECS context mutation
title_controller.render();
if (title_controller.state().SettingsButtonPressed) {
    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Settings;
}
```

## Build and Test Notes

### CMake Integration

The build system integrates rGuiLayout as follows:

```cmake
# 1. Ensure raygui is available (vendored or vcpkg)
target_include_directories(shapeshifter PRIVATE ${RAYGUI_INCLUDE_DIR})

# 2. Add generated layout headers (header-only, automatically included via screen controllers)
# No explicit CMake target needed for .h files.

# 3. Add screen controller sources to the build
target_sources(shapeshifter PRIVATE
    app/ui/screen_controllers/title_screen_controller.cpp
    app/ui/screen_controllers/tutorial_screen_controller.cpp
    # ... add others as they land
)

# 4. Define raygui implementation on exactly one source and keep it out of unity
set_source_files_properties(
    app/ui/screen_controllers/title_screen_controller.cpp
    PROPERTIES
        COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION
        SKIP_UNITY_BUILD_INCLUSION TRUE
)
```

### Compiler Warnings

- Generated headers and raygui implementation must compile **zero warnings** with `-Wall -Wextra -Werror` (Clang, MSVC, Emscripten).
- Screen controllers must also be warning-clean.
- If a generated header produces warnings, regenerate from `.rgl` or adjust the template.

### Testing

- **Unit tests** for screen controllers: verify that button-press flags trigger correct game events.
- **Integration tests** for screen controllers: verify that game state is correctly bound to layout state and displayed.
- **Visual regression tests** (future): compare rendered layouts against baseline screenshots.
- **Keyboard/touch tests** for input handling via raygui immediate-mode API.

## Validation Checklist

- [ ] All screen `.rgl` files are authored and committed in `content/ui/screens/`.
- [ ] All generated `<screen>_layout.h` files are committed in `app/ui/generated/`.
- [ ] No generated files are hand-edited; any layout change regenerates from `.rgl`.
- [ ] All screen controllers include the corresponding generated header and call `Init()` / `Render()`.
- [ ] No screen controller copies layout rectangles, rebuilds layout caches, or spawns UI widget entities.
- [ ] CMakeLists.txt defines `RAYGUI_IMPLEMENTATION` on exactly one source and excludes it from unity.
- [ ] CMakeLists.txt includes raygui and screen controller sources.
- [ ] All screens render without visual glitches or layout data corruption.
- [ ] Button presses dispatch correct game commands; no duplicate events.
- [ ] Build is warning-free (native and WASM).
- [x] Legacy `app/ui/adapters/` folder has been migrated to `app/ui/screen_controllers/` (migration complete; all 8 screens implemented as screen controllers).

## Open Decisions

1. **Template completeness:** Does rguilayout's `embeddable_layout.h` template require manual post-processing, or does it auto-generate fully? If manual, document the process and consider a helper script.
2. **Dynamic content:** For screens with per-frame text updates (score, selected song), does the generated header expose setter functions, or do screen controllers directly modify state fields?
3. **Platform guards:** Should screen controllers apply web/desktop visibility rules inline, or should the generated layout respect platform flags?
4. **Preloading:** For the gameplay HUD, should layout state be pre-allocated and reused across game phases, or recreated per-phase?

## Summary

**Portable generated C is the primary integration contract.** rGuiLayout produces self-contained, reusable header files that the game consumes via thin screen-controller code. Screen controllers translate raygui input into game commands and bind dynamic game state to layout text/values, but do not build or cache layout state. This separation keeps generated code portable, screen controller code minimal and focused, and game code independent of UI layout details.
