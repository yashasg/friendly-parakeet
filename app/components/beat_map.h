#pragma once

#include "obstacle.h"
#include "player.h"
#include <cstdint>
#include <string>
#include <vector>

// ── Beat Entry (one entry in the beat map) ──────────
struct BeatEntry {
    int          beat_index   = 0;
    ObstacleKind kind         = ObstacleKind::ShapeGate;
    Shape        shape        = Shape::Circle;
    int8_t       lane         = 1;
    float        time_sec     = 0.0f;
    bool         has_time_sec = false;
};

// ── Beat Map (the full chart, loaded from JSON) ─────
// BPM is fixed for the entire song — no mid-song tempo changes.
//
// Ownership / lifecycle:
//   - Attached to the singleton BeatMapTag entity created by create_beat_map_entity().
//   - Populated (and reset) at the start of each play session in setup_play_session().
//   - Read-only during active gameplay by all systems that need beat timing.
//   - Implicitly freed when the registry is destroyed.
//
// This is a cold asset singleton. Copy construction and copy assignment are
// deleted so accidental duplication of the heap-allocated beat array is a
// compile-time error. Use std::move() when transferring ownership.
struct BeatMap {
    std::string song_id;
    std::string title;
    std::string song_path;
    float       bpm        = 120.0f;
    float       offset     = 0.0f;
    int         lead_beats = 4;
    float       duration   = 180.0f;
    std::string difficulty;
    std::vector<float> beat_times;

    std::vector<BeatEntry> beats;

    BeatMap()                          = default;
    BeatMap(const BeatMap&)            = delete;
    BeatMap& operator=(const BeatMap&) = delete;
    BeatMap(BeatMap&&)                 = default;
    BeatMap& operator=(BeatMap&&)      = default;
};
