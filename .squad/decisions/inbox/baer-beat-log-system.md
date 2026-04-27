# Decision: beat_log_system placement and state ownership (#283)

**Date:** 2026-04-27  
**Author:** Baer  
**Status:** IMPLEMENTED (commit c2720ea)

## Decision

Beat logging is extracted from `song_playback_system` into a new `beat_log_system` that lives in `session_logger.cpp`. The system follows the `ring_zone_log_system` precedent exactly.

## State ownership

`SessionLog::last_logged_beat` (int, default -1) tracks the highest beat index already emitted to the log file. This field belongs to logging, not playback. `song_playback_system` never touches it.

## Execution order

`beat_log_system` runs immediately after `song_playback_system` in the fixed-tick loop, before `beat_scheduler_system`. This ensures beat log entries appear before any scheduler action triggered by that beat.

## Test seam

`SessionLog` stored in `reg.ctx()`. Tests open a `/dev/null`-backed file so no disk I/O occurs. Nine `[beat_log]` Catch2 tests cover: no-op paths (absent log, null file, wrong phase, song not playing), forward advance, multi-beat catch-up, no-re-log, and the critical separation proof (playback does not advance last_logged_beat).
