#pragma once

#include "player.h"
#include <cstdint>
#include <string>
#include <vector>

// ── Beat Entry (one row in a per-kind beat vector) ──────────
// The former `ObstacleKind kind` discriminator (issue #1202/#1204) was
// removed: the kind is identified by which per-kind vector in `BeatMap`
// the row lives in (`shape_gate_beats` / `split_path_beats` /
// `onset_marker_beats`). For OnsetMarker rows `shape` is unused (defaults
// to Circle); for ShapeGate rows `lane` is positional only; for SplitPath
// rows `lane` is the required dodge lane.
struct BeatEntry {
    int    beat_index   = 0;
    Shape  shape        = Shape::Circle;
    int8_t lane         = 1;
    float  time_sec     = 0.0f;
    bool   has_time_sec = false;
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
//
// The chart is normalized per Fabian's relational mechanic (#1204): each
// former `ObstacleKind` value gets its own table (vector). Cross-kind
// ordering (e.g. monotonic beat indices) is validated by merging the
// per-kind vectors in beat-index order; intra-kind rules walk only the
// relevant vector. Each per-kind vector is independently `stable_sort`-ed
// by beat_index after parsing.
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

    std::vector<BeatEntry> shape_gate_beats;
    std::vector<BeatEntry> split_path_beats;
    std::vector<BeatEntry> onset_marker_beats;

    BeatMap()                          = default;
    BeatMap(const BeatMap&)            = delete;
    BeatMap& operator=(const BeatMap&) = delete;
    BeatMap(BeatMap&&)                 = default;
    BeatMap& operator=(BeatMap&&)      = default;
};
