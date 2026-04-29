# Skill: C++ Template Adapter Pattern

## Overview
Reusable pattern for eliminating boilerplate in systems that wrap external library state (e.g., raygui layouts, ImGui contexts, SDL windows). Uses C++17 auto template parameters for zero-overhead, type-safe state management.

## When to Use
- **3+ classes with identical lifecycle patterns** (init, render/update, state access)
- State managed by external C library (function pointers for init/render)
- Need compile-time dispatch (no virtual function overhead)
- Each instance needs unique state (no global singletons)

## Pattern Structure

### Template Definition (adapter_base.h)
```cpp
template<typename State, auto InitFunc, auto RenderFunc>
class Adapter {
    State state_;
    bool initialized_ = false;

public:
    void init() {
        if (!initialized_) {
            state_ = InitFunc();
            initialized_ = true;
        }
    }

    void render() {
        if (!initialized_) init();
        RenderFunc(&state_);
    }

    State& state() { return state_; }
    const State& state() const { return state_; }
};
```

### Usage (concrete_adapter.cpp)
```cpp
#include "adapter_base.h"
#include "external_lib.h"  // defines FooState, Foo_Init(), Foo_Render()

namespace {
    using FooAdapter = Adapter<FooState, &Foo_Init, &Foo_Render>;
    FooAdapter foo_adapter;  // unique name for unity builds
}

void foo_adapter_init() {
    foo_adapter.init();
}

void foo_adapter_render(SomeRegistry& reg) {
    foo_adapter.render();
    // Additional dispatch logic using foo_adapter.state()
}
```

## Real-World Example

**Project:** SHAPESHIFTER (bullethell game)
**Context:** 8 raygui screen adapters eliminated 377 lines of duplication
**Files:** `app/ui/adapters/adapter_base.h`, `app/ui/adapters/game_over_adapter.cpp`
**Commit:** 958a7d9 (approved by Keaton 2026-04-29)

---

**Author:** Keaton  
**Date:** 2026-04-29
