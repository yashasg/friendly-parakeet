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
