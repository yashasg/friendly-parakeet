#pragma once

// rhythm.h — Per-entity rhythm components, timing enums, and helpers.
// Loaded-once data (BeatMap) lives in beat_map.h.
// Runtime singletons (SongState, EnergyState, SongResults) live in song_state.h.
// This header re-exports both for backward compatibility.

#include "beat_map.h"
#include "song_state.h"
#include "player.h"
#include "obstacle.h"
#include <cstdint>

// ── Window Phase ────────────────────────────────────
enum class WindowPhase : uint8_t {
    Idle     = 0,
    MorphIn  = 1,
    Active   = 2,
    MorphOut = 3
};

// ── Timing Grade (emplaced on obstacle at collision) ─
#define TIMING_TIER_LIST(X) \
    X(Bad)                  \
    X(Ok)                   \
    X(Good)                 \
    X(Perfect)

enum class TimingTier : uint8_t {
    #define TIMING_TIER_ENUM(name) name,
    TIMING_TIER_LIST(TIMING_TIER_ENUM)
    #undef TIMING_TIER_ENUM
};

inline const char* ToString(TimingTier t) {
    switch (t) {
        #define TIMING_TIER_STR(name) case TimingTier::name: return #name;
        TIMING_TIER_LIST(TIMING_TIER_STR)
        #undef TIMING_TIER_STR
    }
    return "???";
}

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

// ── Helper: window scale factor from tier ────────────
inline float window_scale_for_tier(TimingTier tier) {
    switch (tier) {
        case TimingTier::Perfect: return 1.50f;
        case TimingTier::Good:    return 1.00f;
        case TimingTier::Ok:      return 0.75f;
        case TimingTier::Bad:     return 0.50f;
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
