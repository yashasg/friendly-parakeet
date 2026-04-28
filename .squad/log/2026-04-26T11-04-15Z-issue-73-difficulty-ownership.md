# Issue #73: Reconcile time-based difficulty ramp with song-driven beatmap spawning

**Timestamp:** 2026-04-26T11:04:15Z  
**Squad:** Ralph (implementation), Coordinator (tightening/validation), Kujan (review), Scribe (logging)

## Scope

Resolved ambiguity between authored song difficulties and the legacy elapsed-time random-spawn difficulty ramp.

## Outcome

- Song-mode difficulty is owned by selected authored beatmap variants:
  - `easy`
  - `medium`
  - `hard`
- Level Select UI labels map directly to `LevelSelectState::DIFFICULTY_KEYS`.
- `setup_play_session()` passes the selected key to `load_and_validate_beat_map()`.
- `beat_map_loader.cpp` loads the matching `difficulties.<key>.beats` array.
- `difficulty_system()` returns early while `SongState.playing == true`.
- `obstacle_spawn_system()` returns early while `SongState.playing == true`.
- Freeplay/random spawning owns the elapsed-time `DifficultyConfig` ramp and remains dev/prototype/post-release only.

## Coordinator tightening

- Removed stale GDD language saying current gameplay speed increases over time.
- Replaced stale speed-bar HUD references with energy/proximity-ring language.
- Updated prototype examples that still displayed speed-ramp HUD values.
- Confirmed remaining speed-bar mentions in `rhythm-spec.md` are explicit removal notes.

## Validation

- `git diff --check` on changed #73 files
- Native CMake configure in verify directory
- `cmake --build <verify-dir> --target shapeshifter shapeshifter_tests`
- Focused tests: 130 assertions in 56 test cases
- Full native non-benchmark suite: 2487 assertions in 759 test cases

## Review

Kujan approved with no changes requested.

## GitHub

Comment posted to issue #73.
