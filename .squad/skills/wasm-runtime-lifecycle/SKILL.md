---
name: "wasm-runtime-lifecycle"
description: "Keep Emscripten game loops responsive by separating runtime keepalive from shutdown ownership and validating linker lifecycle flags in CI."
domain: "wasm, cmake, runtime"
confidence: "high"
source: "earned"
---

## Context
Browser game loops are long-running callbacks. If shutdown ownership is split between `main()` and platform callbacks, or if runtime keepalive depends on unwind semantics, previews can become unresponsive or teardown unexpectedly.

## Pattern

1. **Web `main()` should not do post-loop teardown.**
   - Call init and run.
   - Skip immediate shutdown under `__EMSCRIPTEN__`.

2. **Centralize WASM shutdown in platform hooks.**
   - Quit path in frame callback.
   - `beforeunload` fallback.
   - Idempotent shutdown sentinel.

3. **Avoid unwind-dependent loop ownership.**
   - Use `emscripten_set_main_loop(..., simulate_infinite_loop=0)`.
   - Keep runtime alive with linker flag `-sNO_EXIT_RUNTIME=1`.

4. **CI guard linker lifecycle flags.**
   - Fail build if `link.txt` lacks `-sASYNCIFY` (for async stack support when needed).
   - Fail build if `link.txt` lacks `-sNO_EXIT_RUNTIME=1`.

## Validation Checklist
- Native configure/build/tests still pass.
- WASM configure/build passes using vcpkg + chainloaded Emscripten toolchain.
- `ctest` executes `shapeshifter_tests_wasm` successfully.
- `build-web/CMakeFiles/shapeshifter.dir/link.txt` contains both lifecycle flags.
