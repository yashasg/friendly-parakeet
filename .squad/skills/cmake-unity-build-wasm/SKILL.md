---
name: "cmake-unity-build-wasm"
description: "How to safely enable CMake unity builds for Emscripten/WASM while keeping native builds unchanged and avoiding ODR hazards in test files."
domain: "cmake, wasm, performance"
confidence: "high"
source: "earned"
---

## Context

When Emscripten compiles C++ source files, each `emcc` invocation carries ~3s of fixed overhead. A project with 40+ source files can spend 120+ seconds on overhead alone before any real compilation, leading to 10+ minute WASM builds. Unity builds (a.k.a. "jumbo builds") merge multiple `.cpp` files into a single TU, dramatically cutting the number of `emcc` invocations.

Native builds should NOT auto-enable unity builds because incremental rebuild speed matters — touching one file recompiles the entire unity batch.

## Patterns

### 1. Auto-enable unity builds for Emscripten only (CMakeLists.txt)

```cmake
option(SHAPESHIFTER_UNITY_BUILD "Enable unity builds for faster compilation" OFF)
# Emscripten pays ~3s per-file overhead; batching TUs cuts WASM build time from
# 10+ min to ~3 min. Native builds keep unity OFF (incremental rebuilds matter).
if(EMSCRIPTEN)
    set(CMAKE_UNITY_BUILD ON)
else()
    set(CMAKE_UNITY_BUILD ${SHAPESHIFTER_UNITY_BUILD})
endif()
```

### 2. Apply test file exclusions for ODR hazards

Before `add_executable(... ${TEST_SOURCES})`, mark unsafe files:

```cmake
set_source_files_properties(
    tests/file_with_duplicate_symbol_a.cpp
    tests/file_with_duplicate_symbol_b.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)
```

Files that need this treatment:
- Any test file using a **per-file static helper** that the same function name in another test file (e.g. `static Path find_test_fixtures()`)
- Any test file with an **anonymous namespace** containing a name that duplicates another test file's anonymous namespace symbol

### 3. Bump CI cache key version on first enable

```yaml
# ci-wasm.yml
key: cmake-web-emscripten-v3-${{ hashFiles('CMakeLists.txt', 'vcpkg.json', ...) }}
restore-keys: |
  cmake-web-emscripten-v3-
```

Old non-unity object files are incompatible with unity builds. Bumping the version prefix ensures a clean build on first run.

### 4. Add parallel jobs to cmake --build

```yaml
cmake --build build-web -- -j$(nproc)
```

Unity build consolidates TUs, but parallel linking still benefits from `-j`.

## Examples

From this project (`bullethell`):
- `CMakeLists.txt` lines 24–33: Emscripten detection → `CMAKE_UNITY_BUILD ON`
- `CMakeLists.txt` after `TEST_SOURCES` glob: `set_source_files_properties(... SKIP_UNITY_BUILD_INCLUSION TRUE)` for 10 hazardous test files
- `.github/workflows/ci-wasm.yml`: cache key v3, `-j$(nproc)`

## Anti-Patterns

- **Global `CMAKE_UNITY_BUILD ON` without EMSCRIPTEN guard**: Breaks native incremental builds; slows developer iteration.
- **Skipping hazard audit**: If two test files both define `static void setup()` or `namespace { int g = 0; }`, unity build will produce redefinition errors. Always grep for same-named symbols.
- **Not bumping cache key**: Stale cached `.o` files from non-unity builds can confuse the linker or produce silent wrong-code bugs.
- **Forgetting `-j$(nproc)` on `cmake --build`**: Unity build reduces number of TUs but doesn't automatically parallelize — must still pass job count.
