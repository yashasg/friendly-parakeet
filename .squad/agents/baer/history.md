# Baer — History (SUMMARIZED)

**Last Updated:** 2026-05-10  
**Current Focus:** Cleanup and refactoring audit  
**Status:** Recent validation and decision consolidation work

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Test Engineer
- **Joined:** 2026-04-26T02:12:00.632Z

## 2026-05-10T02:36:13.250-07:00 — Automated test coverage audit

**Status:** AUDIT COMPLETE (issues filed)

**Validation evidence:**
- Full native validation command passed: `cmake -B build -S . -Wno-dev && cmake --build build && ./build/shapeshifter_tests`.
- Python tool lane is not wired into CMake/run.sh/CI and currently fails manually: `python3 -m unittest tools/test_*.py` errors on stale `tools/test_get_audio_duration.py` import of removed `FFPROBE_TIMEOUT`/`get_audio_duration`.
- `tests/test_lifecycle.cpp` and `tests/test_safe_area_layout.cpp` are tracked but excluded by `CMakeLists.txt`, and their named cases are absent from `shapeshifter_tests --list-tests`.
- Shipped beatmaps currently contain onset metadata (`timing_source=onset`, broad `onset_class`, `onset_time_sec`, `subdivision_label`), but no native shipped-content test enforces those metadata invariants.

**Issues filed:**
- #397 — Re-enable tracked tests excluded from CMake discovery.
- #398 — Add shipped beatmap regression gate for onset metadata invariants.
- #399 — Wire Python tool tests into validation and fix stale audio-duration coverage.

**Learning:** Keep generator/tool Python tests as a first-class validation lane when they own content invariants; otherwise onset-layer guarantees can silently depend on tests that the standard green build never executes.

## 2026-05-10T09:49:00Z — Test coverage fixes for #397 #398 #399

## Learnings
- Re-enabling tracked lifecycle/safe-area tests required modernizing stale assertions to current `constants.h` fields; keeping the original test names/tags preserved discoverability gates while restoring build stability.
- A shipped-content metadata regression test is now necessary alongside structural beatmap tests: assert `timing_source=onset`, required onset metadata fields, broad `onset_class` values, and non-decreasing onset-backed counts across easy/medium/hard.
- Python tool validation is now wired in both CTest (`python_tool_tests`) and `run.sh test`; stale ffprobe-based coverage was replaced with duration-contract tests against `rhythm_pipeline.extract_features` so `python3 -m unittest discover -s tools -p "test_*.py"` stays green.

## 2026-05-10T02:40:52.785-07:00 — Round 2 CI/test wiring audit

## Learnings
- `python_tool_tests` is correctly registered in native CTest, but current native CI workflows still execute only `./build/shapeshifter_tests "~[bench]"`, so the Python `tools/test_*.py` suite is not part of blocking CI.
- Because native PR workflows also ignore `tools/**`, regressions in tool tests can merge without any CI signal unless a dedicated tool-test lane (or CTest-based native test step) is added.
