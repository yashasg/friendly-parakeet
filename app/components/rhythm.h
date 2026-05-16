#pragma once

// rhythm.h — Per-entity rhythm components.
// Loaded-once data (BeatMap) lives in beat_map.h.
// Runtime singletons: SongState / SongResults live in song_state.h;
// EnergyState lives in energy_bar.h. This header re-exports all three for
// backward compatibility.

#include "beat_map.h"
#include "song_state.h"
#include "energy_bar.h"
#include "player.h"
#include "obstacle.h"

// ── Timing Grade (emplaced on obstacle at collision) ─
// Per-row "precision" scalar carried by every graded obstacle. The tier
// discriminator is a per-tier tag in app/tags/tags.h (TimingPerfectTag /
// TimingGoodTag / TimingOkTag / TimingBadTag) — issue #1202/#1204.
struct TimingGrade {
    float precision = 0.0f;  // 0.0 = edge, 1.0 = dead center
};

// ── Beat Info (per-obstacle, carries chart data) ─────
struct BeatInfo {
    int   beat_index   = 0;
    float arrival_time = 0.0f;
    float spawn_time   = 0.0f;
};
