#pragma once

#include <cstddef>
#include "../constants.h"

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

    // ── Derived fields (computed once by song_state_compute_derived) ────────
    // Recalculated from bpm/lead_beats at session init; read-only during play.
    float beat_period     = 0.5f;   // seconds per beat
    float lead_time       = 2.0f;   // total approach window in seconds
    float scroll_speed    = 520.0f; // pixels per second (APPROACH_DIST / lead_time)
    float window_duration = 0.8f;   // hit-window width in seconds
    float half_window     = 0.4f;   // window_duration / 2
    float morph_duration  = 0.1f;   // shape-morph animation length in seconds

    // ── Per-frame mutable fields ─────────────────────────────────────────────
    float  song_time     = 0.0f;   // mutated every frame by song_playback_system
    int    current_beat  = -1;     // mutated every frame by song_playback_system
    bool   playing       = false;  // set true by setup_play_session; cleared by
                                   //   song_playback_system (end-of-song) or energy_system (death)
    bool   finished      = false;  // set true by song_playback_system or energy_system
    bool   restart_music = false;  // set true by setup_play_session; consumed/cleared
                                   //   on the next tick by song_playback_system

    // ── Beat-schedule cursor (per-frame, reset at session init) ─────────────
    size_t next_spawn_idx = 0;     // advanced each frame by beat_scheduler_system
};

// ── Energy State (singleton) ────────────────────────
struct EnergyState {
    float energy      = 1.0f;   // [0.0, 1.0] — current energy
    float display     = 1.0f;   // smoothed for rendering (lerps toward energy)
    float flash_timer = 0.0f;   // > 0 when bar should flash (drain event)
};

// ── Game Over Cause (singleton) ─────────────────────
// Tracks the most recent reason the player's run ended.  Set by the
// system that triggered the end-of-run condition (collision or energy)
// and read by the Game Over screen to surface a one-line, platform-
// neutral, colorblind-safe reason.
enum class DeathCause : uint8_t {
    None = 0,
    EnergyDepleted = 1,
    MissedABeat    = 2,
    HitABar        = 3,
};

struct GameOverState {
    DeathCause cause = DeathCause::None;
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

// ── Helper: compute derived SongState fields from BPM ─
inline void song_state_compute_derived(SongState& s) {
    s.beat_period     = 60.0f / s.bpm;
    s.lead_time       = s.lead_beats * s.beat_period;

    s.scroll_speed    = constants::APPROACH_DIST / s.lead_time;

    constexpr float BASE_WINDOW_BEATS = 1.6f;
    constexpr float MIN_WINDOW        = 0.36f;
    constexpr float BASE_MORPH_BEATS  = 0.2f;
    constexpr float MIN_MORPH         = 0.06f;

    float bpm_scale = (s.bpm > 130.0f) ? (130.0f / s.bpm) : 1.0f;
    s.window_duration = BASE_WINDOW_BEATS * s.beat_period * bpm_scale;
    if (s.window_duration < MIN_WINDOW) s.window_duration = MIN_WINDOW;
    s.half_window     = s.window_duration / 2.0f;

    s.morph_duration  = BASE_MORPH_BEATS * s.beat_period;
    if (s.morph_duration < MIN_MORPH) s.morph_duration = MIN_MORPH;
}
