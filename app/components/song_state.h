#pragma once

#include "../constants.h"

#include <cstddef>

// EnergyState lives in components/energy_bar.h (gameplay-resource singleton).
// EnergyDepletedDeath (and future *Death ctx tags) live in components/game_state.h
// (terminal-phase data).
// Only song-lifecycle singletons (timing/playback state and song-scope result
// tallies) remain in this header.

// ── Song State (singleton, lives in registry context) ─
// Emplaced once in game_loop.cpp; reset and populated by setup_play_session()
// (play_session.cpp) via init_song_state() at the start of each session.
struct SongState {
    // ── Session-init fields (set by init_song_state / setup_play_session) ──
    // Copied from BeatMap; fixed for the lifetime of a session.
    float bpm             = 120.0f;
    float offset          = 0.0f;
    int   lead_beats      = 4;
    float duration_sec    = 180.0f;

    // ── Derived fields (computed once by util/rhythm_math.h) ────────────────
    // Recalculated from bpm/lead_beats at session init; read-only during play.
    float beat_period     = 0.5f;   // seconds per beat
    float lead_time       = 2.0f;   // total approach window in seconds
    float scroll_speed    = constants::APPROACH_DIST / lead_time; // pixels per second
    float window_duration = 0.3f;   // hit-window width in seconds
    float half_window     = 0.15f;  // window_duration / 2
    float morph_duration  = 0.1f;   // shape-morph animation length in seconds

    // ── Per-frame mutable fields ─────────────────────────────────────────────
    float  song_time     = 0.0f;   // mutated every frame by song_playback_system
    int    current_beat  = -1;     // mutated every frame by song_playback_system
    bool   playing       = false;  // set true by setup_play_session; cleared by
                                   //   song_playback_system or terminal GameOver entry
    bool   finished      = false;  // set true by song_playback_system or terminal GameOver entry
    bool   restart_music = false;  // set true by setup_play_session; consumed/cleared
                                   //   on the next tick by song_playback_system

    // ── Beat-schedule cursors (per-(kind, shape, timing-source), reset at session init) ──
    // Replaces the former `next_spawn_idx` single cursor (issue #1202/#1204).
    // Each former enum value (`ObstacleKind`, `Shape`) was a hidden lookup
    // table; per Fabian's relational-database mechanic, each value gets its
    // own row in BeatMap and its own cursor here. The further `_timed_idx`
    // split (#1533) tracks the per-bin authored-onset siblings introduced
    // when `BeatEntry::has_time_sec` was eradicated: indexed entries live in
    // `*_beats` and timed entries live in `*_beats_timed`, each with an
    // independent cursor. beat_scheduler_system runs one per-cursor transform;
    // each cursor advances independently over its own vector in BeatMap.
    // Shape::Hexagon is not a valid required shape for shape_gate / split_path,
    // so no `*_hexagon_idx` cursor exists.
    size_t next_shape_gate_circle_idx          = 0;
    size_t next_shape_gate_square_idx          = 0;
    size_t next_shape_gate_triangle_idx        = 0;
    size_t next_split_path_circle_idx          = 0;
    size_t next_split_path_square_idx          = 0;
    size_t next_split_path_triangle_idx        = 0;
    size_t next_onset_marker_idx               = 0;
    size_t next_shape_gate_circle_timed_idx    = 0;
    size_t next_shape_gate_square_timed_idx    = 0;
    size_t next_shape_gate_triangle_timed_idx  = 0;
    size_t next_split_path_circle_timed_idx    = 0;
    size_t next_split_path_square_timed_idx    = 0;
    size_t next_split_path_triangle_timed_idx  = 0;
    size_t next_onset_marker_timed_idx         = 0;
};

// ── Song Results (singleton, accumulates during play) ─
struct SongResults {
    int   perfect_count = 0;
    int   good_count    = 0;
    int   ok_count      = 0;
    int   bad_count     = 0;
    int   miss_count    = 0;
    int   max_chain     = 0;
    int   total_notes   = 0;
};
