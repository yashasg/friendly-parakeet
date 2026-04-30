# Hockney Decision — PR #357 WASM async support

## Context
PR #357 preview crashed in-browser with:
`Aborted(Please compile your program with async support in order to use asynchronous operations like emscripten_sleep)`.

## Decision
For Emscripten builds, `shapeshifter` must link with `-sASYNCIFY`.

## Why
The runtime path uses async browser operations that can trigger `emscripten_sleep` semantics. Without asyncify instrumentation, the generated JS/WASM aborts at runtime even when CI compile/test is green.

## Guardrail
CI workflow `ci-wasm.yml` now validates `build-web/CMakeFiles/shapeshifter.dir/link.txt` contains `-sASYNCIFY`, preventing future silent regressions.

## Scope
WebAssembly target config only; no gameplay/system behavior changes.
