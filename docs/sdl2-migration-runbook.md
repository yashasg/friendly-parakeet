# SDL2 Migration Runbook (Issue #372)

Phase 7 finalization is complete: SDL2 is now the only active backend path and raylib is no longer a runtime dependency.

## Migration-complete validation matrix

Legend:
- ✅ required for migration acceptance
- ◻️ run when lane is available (not always local)

| Platform lane | Configure + build | Core regression suite | Migration parity slice | Known-failure sentinel |
|---|---|---|---|---|
| macOS native (local) | `cmake -B build -S . -DSHAPESHIFTER_BACKEND=sdl2 -DCMAKE_BUILD_TYPE=Release -Wno-dev && cmake --build build --target shapeshifter_tests` ✅ | `./build/shapeshifter_tests --skip-benchmarks -v quiet` ✅ | `./build/shapeshifter_tests "[render][sdl2][validation]" -v quiet` ✅ | `ctest --test-dir build --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` ✅ (expected fail) |
| Linux CI | same as macOS with Linux generator ◻️ | `./build/shapeshifter_tests --skip-benchmarks -v quiet` ◻️ | `./build/shapeshifter_tests "[render][sdl2][validation]" -v quiet` ◻️ | same `ctest -R "redfoot/#168..."` ◻️ |
| Windows CI | same backend flags on Windows generator ◻️ | `build\\shapeshifter_tests.exe --skip-benchmarks -v quiet` ◻️ | `build\\shapeshifter_tests.exe "[render][sdl2][validation]" -v quiet` ◻️ | `ctest --test-dir build --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"` ◻️ |
| WASM CI | `emcmake cmake -B build-web -S . -DSHAPESHIFTER_BACKEND=sdl2 ... && cmake --build build-web --target shapeshifter` ◻️ | `cd build-web && ctest --verbose --output-on-failure` ◻️ | linker flag validation (`-sUSE_SDL=2`, `-sASYNCIFY`, `-sNO_EXIT_RUNTIME=1`) ◻️ | N/A |

## Baseline pre-existing failure ledger

1. **Known functional failure (pre-existing):**
   - Test: `redfoot/#168: existing game_over buttons keep their original positions`
   - Repro:  
     `ctest --test-dir build --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"`
   - Current behavior: fails at `tests/test_redfoot_testflight_ui.cpp:67` (`REQUIRE(restart.has_value())`).

## Repeatable migration-complete acceptance commands (local)

```bash
# 1) Configure + build
cmake -B build -S . -DSHAPESHIFTER_BACKEND=sdl2 -DCMAKE_BUILD_TYPE=Release -Wno-dev
cmake --build build --target shapeshifter_tests

# 2) Core regression gate (no benchmarks)
./build/shapeshifter_tests --skip-benchmarks -v quiet

# 3) SDL2 deterministic render validation
./build/shapeshifter_tests "[render][sdl2][validation]" -v quiet

# 4) Known-failure sentinel
ctest --test-dir build --output-on-failure -R "redfoot/#168: existing game_over buttons keep their original positions"
```
