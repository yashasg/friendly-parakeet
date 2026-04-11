#pragma once

#include "player.h"
#include "obstacle.h"
#include <cstdint>
#include <string>
#include <vector>

// ── Window Phase ────────────────────────────────────
enum class WindowPhase : uint8_t {
    Idle     = 0,
    MorphIn  = 1,
    Active   = 2,
    MorphOut = 3
};

// ── Timing Grade (emplaced on obstacle at collision) ─
enum class TimingTier : uint8_t {
    Bad     = 0,
    Ok      = 1,
    Good    = 2,
    Perfect = 3
};

struct TimingGrade {
    TimingTier tier      = TimingTier::Bad;
    float      precision = 0.0f;  // 0.0 = edge, 1.0 = dead center
};

// ── Beat Info (per-obstacle, carries chart data) ─────
struct BeatInfo {
    int   beat_index   = 0;
    float arrival_time = 0.0f;
    float spawn_time   = 0.0f;
};

// ── Beat Entry (one entry in the beat map) ──────────
struct BeatEntry {
    int          beat_index   = 0;
    ObstacleKind kind         = ObstacleKind::ShapeGate;
    Shape        shape        = Shape::Circle;
    int8_t       lane         = 1;
    uint8_t      blocked_mask = 0;
};

// ── Beat Map (the full chart, loaded from JSON) ─────
// BPM is fixed for the entire song — no mid-song tempo changes.
struct BeatMap {
    std::string song_id;
    std::string title;
    std::string song_path;
    float       bpm        = 120.0f;
    float       offset     = 0.0f;
    int         lead_beats = 4;
    float       duration   = 180.0f;
    std::string difficulty;

    std::vector<BeatEntry> beats;
};

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
    bool   restart_music = false;  // signal: enter_playing() → song_playback_system

    // Beat schedule cursor
    size_t next_spawn_idx = 0;
};

// ── HP State (singleton) ────────────────────────────
struct HPState {
    int current = 5;
    int max_hp  = 5;
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

    constexpr float APPROACH_DIST = 1040.0f; // PLAYER_Y - SPAWN_Y
    s.scroll_speed    = APPROACH_DIST / s.lead_time;

    constexpr float BASE_WINDOW_BEATS = 1.6f;
    constexpr float MIN_WINDOW        = 0.36f;
    constexpr float BASE_MORPH_BEATS  = 0.2f;
    constexpr float MIN_MORPH         = 0.06f;

    s.window_duration = BASE_WINDOW_BEATS * s.beat_period;
    if (s.window_duration < MIN_WINDOW) s.window_duration = MIN_WINDOW;
    s.half_window     = s.window_duration / 2.0f;

    s.morph_duration  = BASE_MORPH_BEATS * s.beat_period;
    if (s.morph_duration < MIN_MORPH) s.morph_duration = MIN_MORPH;
}

// ── Helper: window scale factor from tier ────────────
inline float window_scale_for_tier(TimingTier tier) {
    switch (tier) {
        case TimingTier::Perfect: return 0.50f;
        case TimingTier::Good:    return 0.75f;
        case TimingTier::Ok:      return 1.00f;
        case TimingTier::Bad:     return 1.00f;
    }
    return 1.00f;
}

// ── Helper: timing multiplier from tier ─────────────
inline float timing_multiplier(TimingTier tier) {
    switch (tier) {
        case TimingTier::Perfect: return 1.50f;
        case TimingTier::Good:    return 1.00f;
        case TimingTier::Ok:      return 0.50f;
        case TimingTier::Bad:     return 0.25f;
    }
    return 0.25f;
}

// ── Helper: compute timing tier from pct_from_peak ──
inline TimingTier compute_timing_tier(float pct_from_peak) {
    if (pct_from_peak <= 0.25f) return TimingTier::Perfect;
    if (pct_from_peak <= 0.50f) return TimingTier::Good;
    if (pct_from_peak <= 0.75f) return TimingTier::Ok;
    return TimingTier::Bad;
}
