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

3. **Keep loop semantics compatible with raylib on web.**
   - Use `emscripten_set_main_loop(..., simulate_infinite_loop=1)` on this stack.
   - Keep runtime alive with linker flag `-sNO_EXIT_RUNTIME=1`.
   - Avoid shutdown in `main()` under `__EMSCRIPTEN__` to prevent double-teardown.

4. **CI guard linker lifecycle flags.**
   - Fail build if `link.txt` lacks `-sASYNCIFY` (for async stack support when needed).
   - Fail build if `link.txt` lacks `-sNO_EXIT_RUNTIME=1`.

## Validation Checklist
- Native configure/build/tests still pass.
- WASM configure/build passes using vcpkg + chainloaded Emscripten toolchain.
- `ctest` executes `shapeshifter_tests_wasm` successfully.
- `build-web/CMakeFiles/shapeshifter.dir/link.txt` contains both lifecycle flags.
- Browser smoke verifies **interaction response**, not just loader completion (e.g., screenshot diff before/after safe canvas input).

## Playwright Guardrail
- For `page.waitForFunction`, pass options as the third argument: `waitForFunction(fn, undefined, { timeout })`.
- Use a click coordinate that cannot hit menu chrome/excluded buttons (in this project: `x=200,y=200` on title canvas) to avoid false negatives.
