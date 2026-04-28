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
    uint8_t      blocked_mask = 0;
};

// ── Beat Map (the full chart, loaded from JSON) ─────
// BPM is fixed for the entire song — no mid-song tempo changes.
//
// Ownership / lifecycle:
//   - Emplaced once into the registry context in game_loop.cpp:
//       reg.ctx().emplace<BeatMap>();
//   - Populated (and reset) at the start of each play session in
//     setup_play_session() via reg.ctx().get<BeatMap>().
//   - Read-only during active gameplay by all systems that need beat timing.
//   - Implicitly freed when the registry is destroyed (end of process).
//
// This is a cold asset / context singleton. It MUST NOT be emplaced on
// individual entities. Copy construction and copy assignment are deleted so
// that accidental duplication of the heap-allocated beat array is a
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

    std::vector<BeatEntry> beats;

    BeatMap()                          = default;
    BeatMap(const BeatMap&)            = delete;
    BeatMap& operator=(const BeatMap&) = delete;
    BeatMap(BeatMap&&)                 = default;
    BeatMap& operator=(BeatMap&&)      = default;
};
