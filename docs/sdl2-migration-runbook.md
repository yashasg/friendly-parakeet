# SDL2 Migration Runbook (Issue #372)

This runbook tracks the current dual-backend period while phase 6 continues and phase 7 prep is underway.

## Backends currently supported

- `raylib` (default)
- `sdl2` (migration target)

## Final migration gate parity matrix (phase 5/6 closeout)

Legend:
- ✅ required for migration acceptance
- ◻️ run when lane is available (not always local)

| Backend | Platform lane | Configure + build | Core regression suite | Migration parity slice | Known-failure sentinel |
|---|---|---|---|---|---|
| raylib | macOS native (local) | `cmake -B build-raylib -S . -DSHAPESHIFTER_BACKEND=raylib -DCMAKE_BUILD_TYPE=Release -Wno-dev && cmake --build build-raylib --target shapeshifter_tests` ✅ | `./build-raylib/shapeshifter_tests --skip-benchmarks -v quiet` ✅ | N/A (SDL2-only validation tags) | `ctest --test-dir build-raylib --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` ✅ (expected fail) |
| sdl2 | macOS native (local) | `cmake -B build-sdl2 -S . -DSHAPESHIFTER_BACKEND=sdl2 -DCMAKE_BUILD_TYPE=Release -Wno-dev && cmake --build build-sdl2 --target shapeshifter_tests` ✅ | `./build-sdl2/shapeshifter_tests --skip-benchmarks -v quiet` ✅ | `./build-sdl2/shapeshifter_tests "[render][sdl2][validation]" -v quiet` ✅ | `ctest --test-dir build-sdl2 --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` ✅ (expected fail) |
| raylib | Linux CI | same as macOS with Linux generator ◻️ | `./build-raylib/shapeshifter_tests --skip-benchmarks -v quiet` ◻️ | N/A (SDL2-only validation tags) | same `ctest -R "redfoot/#168..."` ◻️ |
| sdl2 | Linux CI | same as macOS with Linux generator ◻️ | `./build-sdl2/shapeshifter_tests --skip-benchmarks -v quiet` ◻️ | `./build-sdl2/shapeshifter_tests "[render][sdl2][validation]" -v quiet` ◻️ | same `ctest -R "redfoot/#168..."` ◻️ |
| raylib | Windows CI | same backend flags on Windows generator ◻️ | `build-raylib\\shapeshifter_tests.exe --skip-benchmarks -v quiet` ◻️ | N/A (SDL2-only validation tags) | `ctest --test-dir build-raylib --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` ◻️ |
| sdl2 | Windows CI | same backend flags on Windows generator ◻️ | `build-sdl2\\shapeshifter_tests.exe --skip-benchmarks -v quiet` ◻️ | `build-sdl2\\shapeshifter_tests.exe "[render][sdl2][validation]" -v quiet` ◻️ | `ctest --test-dir build-sdl2 --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` ◻️ |

## Baseline pre-existing failure ledger (for migration triage)

Baseline captured on branch `feature/sdl2-migration-phase-1-abstraction-layer` (2026-05-04, local macOS lane).

1. **Known functional failure (both backends, pre-existing):**
   - Test: `redfoot/#168: existing game_over buttons keep their original positions`
   - Repro (raylib):  
     `ctest --test-dir build-raylib --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"`
   - Repro (sdl2):  
     `ctest --test-dir build-sdl2 --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"`
   - Current behavior: fails at `tests/test_redfoot_testflight_ui.cpp:67` (`REQUIRE(restart.has_value())`).

2. **Known harness/infrastructure failure (SDL2 full ctest lane, pre-existing):**
   - Repro:  
     `ctest --test-dir build-sdl2 --output-on-failure`
   - Current behavior: CTest reports many `***Not Run` entries with:
     `Unable to find executable: /Users/yashasgujjar/dev/bullethell/build-sdl2/shapeshifter_tests`
   - Triage note: this is a test-discovery/execution-path issue in the SDL2 ctest lane, not a backend parity signal.

## Repeatable final migration acceptance command set (local)

```bash
# 1) Configure + build both backends
cmake -B build-raylib -S . -DSHAPESHIFTER_BACKEND=raylib -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build-raylib --target shapeshifter_tests
cmake -B build-sdl2 -S . -DSHAPESHIFTER_BACKEND=sdl2 -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build-sdl2 --target shapeshifter_tests

# 2) Core parity gate (migration-sensitive, no benchmarks)
./build-raylib/shapeshifter_tests --skip-benchmarks -v quiet
./build-sdl2/shapeshifter_tests --skip-benchmarks -v quiet

# 3) Render parity deterministic checks (SDL2-only)
./build-sdl2/shapeshifter_tests "[render][sdl2][validation]" -v quiet

# 4) Known-failure sentinels (must remain unchanged unless fixed intentionally)
ctest --test-dir build-raylib --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"
ctest --test-dir build-sdl2 --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"
```

## Guardrails during phase 6/7

- Keep fallback paths until final deprecation gate is explicitly approved.
- No backend-specific warning regressions (`-Werror` remains enforced).
- Do not merge changes that only pass one backend.
