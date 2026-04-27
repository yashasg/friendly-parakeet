#pragma once

#include <cstddef>

// ── Song State (singleton, lives in registry context) ─
struct SongState {
    // Song metadata (loaded from beat map)
    float bpm             = 120.0f;
    float offset          = 0.0f;
    int   lead_beats      = 4;
    float duration_sec    = 180.0f;

    // Derived (computed once on song load)
    float beat_period     = 0.5f;
    float lead_time       = 2.0f;
    float scroll_speed    = 520.0f;
    float window_duration = 0.8f;
    float half_window     = 0.4f;
    float morph_duration  = 0.1f;

    // Playback state
    float  song_time     = 0.0f;
    int    current_beat  = -1;
    bool   playing       = false;
    bool   finished      = false;
    bool   restart_music = false;

    // Beat schedule cursor
    size_t next_spawn_idx = 0;
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
    float best_burnout  = 0.0f;
    int   total_notes   = 0;
};

// ── Helper: compute derived SongState fields from BPM ─
inline void song_state_compute_derived(SongState& s) {
    s.beat_period     = 60.0f / s.bpm;
    s.lead_time       = s.lead_beats * s.beat_period;

    constexpr float APPROACH_DIST = 1040.0f;
    s.scroll_speed    = APPROACH_DIST / s.lead_time;

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
