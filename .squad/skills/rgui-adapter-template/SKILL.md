---
name: "rgui-adapter-template"
description: "C++17 template pattern for eliminating boilerplate in RGUILayout adapters"
domain: "architecture, templates, code-reuse"
confidence: "high"
source: "earned (commit 958a7d9, Keaton review revision)"
---

## Context

When integrating generated RGUILayout screen code (from rGuiLayout tool) into the game's ECS architecture, each screen requires an adapter that:
1. Declares a static layout state variable
2. Guards initialization with a bool flag
3. Calls the generated `Init()` function once
4. Calls the generated `Render()` function every frame
5. Reads button-pressed fields from the state and dispatches to game systems

Without abstraction, this pattern results in ~40-50 lines of duplicated code per screen. With 8 screens, that's 300+ lines of boilerplate.

## Patterns

### 1. RGuiAdapter Template (adapter_base.h)

```cpp
template<typename LayoutState, auto InitFunc, auto RenderFunc>
class RGuiAdapter {
    LayoutState state_;
    bool initialized_ = false;

public:
    void init() {
        if (!initialized_) {
            state_ = InitFunc();
            initialized_ = true;
        }
    }

    void render() {
        if (!initialized_) {
            init();
        }
        RenderFunc(&state_);
    }

    LayoutState& state() { return state_; }
    const LayoutState& state() const { return state_; }
};
```

**Key features:**
- C++17 `auto` template parameters allow passing function pointers directly in template arguments
- Compile-time dispatch to generated Init/Render functions (zero runtime overhead)
- Single point of truth for init guard and lazy initialization logic
- Provides accessor for reading button-pressed fields after render

### 2. Adapter Declaration Pattern

In each `{screen}_adapter.cpp`:

```cpp
#include "adapter_base.h"
#include "../generated/{screen}_layout.h"

namespace {

using ScreenAdapter = RGuiAdapter<ScreenLayoutState,
                                  &ScreenLayout_Init,
                                  &ScreenLayout_Render>;
ScreenAdapter screen_adapter;  // Unique name for unity builds

} // anonymous namespace

void screen_adapter_init() {
    screen_adapter.init();
}

void screen_adapter_render(entt::registry& reg) {
    screen_adapter.render();
    
    // Dispatch logic reads screen_adapter.state()
    if (screen_adapter.state().ButtonPressed) {
        // ... dispatch to GameState or other systems
    }
}
```

**Critical detail:** Name the static instance uniquely (`game_over_adapter`, `title_adapter`, not generic `adapter`) to avoid symbol collisions in CMake unity builds where multiple .cpp files are merged into one translation unit.

### 3. Shared Dispatch Helpers

When multiple screens have identical dispatch logic (e.g., end-screen Restart/LevelSelect/MainMenu buttons), extract to a template helper:

```cpp
// end_screen_dispatch.h
template<typename LayoutState>
inline void dispatch_end_screen_choice(GameState& gs, const LayoutState& state) {
    if (state.RestartButtonPressed)
        gs.end_choice = EndScreenChoice::Restart;
    else if (state.LevelSelectButtonPressed)
        gs.end_choice = EndScreenChoice::LevelSelect;
    else if (state.MenuButtonPressed)
        gs.end_choice = EndScreenChoice::MainMenu;
}
```

Then in `game_over_adapter.cpp` and `song_complete_adapter.cpp`:

```cpp
void game_over_adapter_render(entt::registry& reg) {
    game_over_adapter.render();
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= 0.4f) return;
    dispatch_end_screen_choice(gs, game_over_adapter.state());
}
```

### 4. Raylib Helper for Manual Layout

When adapters need to construct Rectangles with anchor-relative offsets (common in screens with dynamic labels), use a small inline helper:

```cpp
// adapter_base.h
inline Rectangle offset_rect(Vector2 anchor, float x, float y, float w, float h) {
    return (Rectangle){anchor.x + x, anchor.y + y, w, h};
}

// Usage in settings_adapter.cpp
GuiLabel(offset_rect(settings_adapter.state().Anchor01, 252, 560, 216, 77), offset_buf);
```

This replaces manual `(Rectangle){state.Anchor.x + x, state.Anchor.y + y, w, h}` arithmetic that is error-prone and verbose.

## Examples

**Before (boilerplate per screen):**
```cpp
namespace {
    GameOverLayoutState game_over_layout_state;
    bool game_over_initialized = false;
}

void game_over_adapter_init() {
    if (!game_over_initialized) {
        game_over_layout_state = GameOverLayout_Init();
        game_over_initialized = true;
    }
}

void game_over_adapter_render(entt::registry& reg) {
    if (!game_over_initialized) {
        game_over_adapter_init();
    }
    GameOverLayout_Render(&game_over_layout_state);
    // ... dispatch logic
}
```

**After (template-based):**
```cpp
namespace {
using GameOverAdapter = RGuiAdapter<GameOverLayoutState,
                                    &GameOverLayout_Init,
                                    &GameOverLayout_Render>;
GameOverAdapter game_over_adapter;
}

void game_over_adapter_init() {
    game_over_adapter.init();
}

void game_over_adapter_render(entt::registry& reg) {
    game_over_adapter.render();
    // ... dispatch logic using game_over_adapter.state()
}
```

**Result:** ~15 lines reduced to ~8 lines per adapter. With 8 adapters, ~56 lines saved (and one template definition). More importantly, init/render contract is now enforced at compile time.

## Anti-Patterns

### ❌ Generic "adapter" variable name in anonymous namespace

**Problem:** Unity builds merge multiple .cpp files into one translation unit. If all adapters use `adapter` as the variable name, they collide.

```cpp
// BAD - causes redefinition error in unity builds
namespace {
    using TitleAdapter = RGuiAdapter<...>;
    TitleAdapter adapter;  // ❌ Collides with other adapters
}
```

**Fix:** Use unique names per screen:
```cpp
namespace {
    using TitleAdapter = RGuiAdapter<...>;
    TitleAdapter title_adapter;  // ✅ Unique
}
```

### ❌ Cargo-cult std::as_const for EnTT storage access

**Problem:** EnTT v3.16.0 `storage<T>()` returns a reference, not a pointer. Wrapping with `std::as_const(reg).storage<T>()` does not prevent pool creation and adds no value.

```cpp
// BAD - unnecessary wrapper, doesn't work in v3.16.0
const auto* store = std::as_const(reg).storage<UIAnimation>();
```

**Fix:** Use `try_get<T>(entity)` per the codebase convention:
```cpp
// GOOD - idiomatic EnTT v3.16.0 pattern
const auto* anim = reg.try_get<UIAnimation>(entity);
```

### ❌ Duplicating dispatch logic across adapters

**Problem:** End-screen adapters (game_over, song_complete) had byte-for-byte identical button dispatch code.

**Fix:** Extract to a shared template helper that operates on any layout state with the required fields (see `end_screen_dispatch.h` example above).

### ❌ Manual Rectangle arithmetic without helper

**Problem:** Repeating `(Rectangle){anchor.x + x, anchor.y + y, w, h}` is verbose and error-prone (easy to swap x/y or w/h).

**Fix:** Use `offset_rect(anchor, x, y, w, h)` wrapper for clarity and brevity.

## References

- Commit 958a7d9 (refactor(ui): extract adapter boilerplate into C++17 template helpers)
- Files: `app/ui/adapters/adapter_base.h`, `app/ui/adapters/end_screen_dispatch.h`
- All 8 adapters: `game_over_adapter.cpp`, `song_complete_adapter.cpp`, `title_adapter.cpp`, `paused_adapter.cpp`, `tutorial_adapter.cpp`, `gameplay_adapter.cpp`, `level_select_adapter.cpp`, `settings_adapter.cpp`
- Keaton review artifact: `.keaton_review_c7700f8.md`
