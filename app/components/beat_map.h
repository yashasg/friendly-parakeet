#pragma once

#include "player.h"
#include <cstdint>
#include <string>
#include <vector>

// ── Beat Entry (one row in a per-(kind,shape) beat vector) ──────────
// Both former discriminator enums on the row are eradicated (issue #1202/#1204):
//
//   - `ObstacleKind kind` -- identified by which per-kind vector in BeatMap
//     the row lives in (shape_gate / split_path / onset_marker).
//   - `Shape shape` -- identified by which per-shape sub-vector in BeatMap
//     the row lives in (`shape_gate_circle_beats`, ..., `split_path_triangle_beats`).
//     OnsetMarker has no shape -- only one vector `onset_marker_beats`.
//
// For ShapeGate rows `lane` is positional only; for SplitPath rows `lane`
// is the required dodge lane; for OnsetMarker rows `lane` is unused.
struct BeatEntry {
    int    beat_index   = 0;
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
// former `ObstacleKind` and each former `Shape` value gets its own table
// (vector). Cross-table ordering (e.g. monotonic beat indices) is validated
// by merging the per-(kind, shape) vectors in beat-index order; intra-table
// rules walk only the relevant vector. Each vector is independently
// `stable_sort`-ed by beat_index after parsing.
//
// Shape::Hexagon is not a valid required shape for shape_gate / split_path
// (`current_required_shape` returns Hexagon as a sentinel meaning "no
// required shape"); the loader rejects shape_gate / split_path rows whose
// shape parses as Hexagon, so no `*_hexagon_beats` vector exists.
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

    std::vector<BeatEntry> shape_gate_circle_beats;
    std::vector<BeatEntry> shape_gate_square_beats;
    std::vector<BeatEntry> shape_gate_triangle_beats;
    std::vector<BeatEntry> split_path_circle_beats;
    std::vector<BeatEntry> split_path_square_beats;
    std::vector<BeatEntry> split_path_triangle_beats;
    std::vector<BeatEntry> onset_marker_beats;

    BeatMap()                          = default;
    BeatMap(const BeatMap&)            = delete;
    BeatMap& operator=(const BeatMap&) = delete;
    BeatMap(BeatMap&&)                 = default;
    BeatMap& operator=(BeatMap&&)      = default;
};

// Total scoring-eligible (ShapeGate + SplitPath) beat count across the
// six per-(kind, shape) vectors. OnsetMarker beats are non-scorable cues
// and are excluded.
inline size_t beat_map_required_count(const BeatMap& m) noexcept {
    return m.shape_gate_circle_beats.size()
         + m.shape_gate_square_beats.size()
         + m.shape_gate_triangle_beats.size()
         + m.split_path_circle_beats.size()
         + m.split_path_square_beats.size()
         + m.split_path_triangle_beats.size();
}

// Total ShapeGate beat count across the three per-shape vectors.
inline size_t shape_gate_count(const BeatMap& m) noexcept {
    return m.shape_gate_circle_beats.size()
         + m.shape_gate_square_beats.size()
         + m.shape_gate_triangle_beats.size();
}

// Total SplitPath beat count across the three per-shape vectors.
inline size_t split_path_count(const BeatMap& m) noexcept {
    return m.split_path_circle_beats.size()
         + m.split_path_square_beats.size()
         + m.split_path_triangle_beats.size();
}

// Total beat count across all seven per-(kind, shape) vectors.
inline size_t beat_map_total_count(const BeatMap& m) noexcept {
    return beat_map_required_count(m) + m.onset_marker_beats.size();
}

// True iff the beat map has zero authored beats across all seven vectors.
inline bool beat_map_empty(const BeatMap& m) noexcept {
    return beat_map_total_count(m) == 0;
}
