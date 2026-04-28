#pragma once

// rhythm.h — Per-entity rhythm components and timing enums.
// Loaded-once data (BeatMap) lives in beat_map.h.
// Runtime singletons (SongState, EnergyState, SongResults) live in song_state.h.
// This header re-exports both for backward compatibility.

#include "beat_map.h"
#include "song_state.h"
#include "player.h"
#include "obstacle.h"
#include <cstdint>

// ── Window Phase ────────────────────────────────────
// Defined in window_phase.h; re-exported here for consumers of rhythm.h.
#include "window_phase.h"

// ── Timing Grade (emplaced on obstacle at collision) ─
enum class TimingTier : uint8_t {
    Bad,
    Ok,
    Good,
    Perfect,
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
