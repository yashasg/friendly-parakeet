# Decision: SongState and DifficultyConfig field ownership

**Author:** McManus  
**Date:** 2026-04-28  
**Related issue:** #340

## Decision

Formalised field ownership categories for the two main gameplay singleton components:

### SongState
- **Session-init** (copied from BeatMap by `init_song_state`): `bpm`, `offset`, `lead_beats`, `duration_sec`
- **Derived** (computed once at init by `song_state_compute_derived`): `beat_period`, `lead_time`, `scroll_speed`, `window_duration`, `half_window`, `morph_duration`
- **Per-frame** (mutated by systems): `song_time`, `current_beat` — `song_playback_system`; `playing`, `finished` — `song_playback_system` + `energy_system`; `restart_music` — consumed by `song_playback_system`
- **Beat cursor** (per-frame): `next_spawn_idx` — `beat_scheduler_system`

### DifficultyConfig
- All fields are session-initialised by `setup_play_session` and ramped per-frame by `difficulty_system` (`elapsed`, `speed_multiplier`, `scroll_speed`, `spawn_interval`) or decremented by `obstacle_spawn_system` (`spawn_timer`).
- `difficulty_system` **skips** all updates when `SongState::playing` is true (rhythm mode).

## Why this matters

Any new system reading or writing these singletons should respect these ownership boundaries. Per-frame fields must only be written by their designated system; init fields must not be written after session setup.
