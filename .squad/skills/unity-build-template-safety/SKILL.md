# Skill: Unity Build Template Safety

## Context
When introducing header-only template abstractions in codebases using unity builds (batching multiple TUs into single compilation units), must prevent ODR violations from static/anonymous namespace symbols.

## Pattern: Safe Template Headers for Unity Builds

### Template Header Requirements
```cpp
// adapter_base.h
#ifndef ADAPTER_BASE_H
#define ADAPTER_BASE_H

// ✅ Template class definitions are implicitly inline — safe
template<typename State, auto InitFunc, auto RenderFunc>
class Adapter {
    State state_;
public:
    void init() { state_ = InitFunc(); }
    void render() { RenderFunc(&state_); }
};

// ✅ Inline free functions — safe (single definition rule exception)
inline Rectangle helper(float x, float y) {
    return {x, y, 100, 50};
}

#endif
```

### Consumer Pattern (per TU)
```cpp
// title_adapter.cpp
namespace {  // ✅ Anonymous namespace isolates instance per TU

using TitleAdapter = Adapter<TitleState, &TitleLayout_Init, &TitleLayout_Render>;
TitleAdapter title_adapter;  // ✅ Unique name prevents collision

}

void title_adapter_render() {
    title_adapter.render();  // OK: name resolves to local instance
}
```

### Anti-Pattern (Unity Build Collision)
```cpp
// BAD: Multiple TUs using SAME identifier in anonymous namespace
// title_adapter.cpp
namespace { Adapter<...> adapter; }  // ❌

// paused_adapter.cpp
namespace { Adapter<...> adapter; }  // ❌ COLLISION when batched into unity TU
```

### Validation Checklist

When reviewing template-based refactors in unity build projects:

1. **Header-only safety**
   - Templates: always header-only (implicitly inline)
   - Free functions: must be `inline` or `template<...>`
   - Static variables in headers: forbidden (ODR violation)

2. **Instance uniqueness**
   - Search: `grep -n "using.*Adapter" app/**/*.cpp`
   - Verify: each TU using template has unique instance name
   - Example: `title_adapter`, `paused_adapter`, NOT generic `adapter`

3. **Build verification**
   - Enable unity build: `cmake -DCMAKE_UNITY_BUILD=ON`
   - Check unity TUs: `cat build/CMakeFiles/target.dir/Unity/*.cxx`
   - Verify no duplicate definitions: `nm unity_*.o | grep " T " | sort | uniq -d`

4. **CMake exclusions**
    - Files with ODR hazards: `set_source_files_properties(... SKIP_UNITY_BUILD_INCLUSION TRUE)`
    - For single-header libs, prefer source-level ownership:
      - `COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION` on exactly one real TU
      - `SKIP_UNITY_BUILD_INCLUSION TRUE` on that same TU

## When to Apply

- Introducing header-only template utilities in projects with `CMAKE_UNITY_BUILD`
- Refactoring duplicated code into template abstractions
- Reviewing PRs that add new header files used across multiple TUs

## References

- Shapeshifter commit 958a7d9: UI adapter template refactor (Keyser)
- CMakeLists.txt: unity build option + source-level `RAYGUI_IMPLEMENTATION` owner (`title_screen_controller.cpp`) with unity exclusion
- Hockney history: Unity build WASM session, Keaton hazard audit coordination
