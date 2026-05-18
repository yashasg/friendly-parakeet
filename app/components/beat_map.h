#pragma once

#include "player.h"
#include <cstdint>
#include <string>
#include <vector>

// ── Beat Entry (one row in a per-(kind,shape) beat vector) ──────────
// All three former discriminator/optional fields on the row are eradicated:
//
//   - `ObstacleKind kind` -- identified by which per-kind vector in BeatMap
//     the row lives in (shape_gate / split_path / onset_marker) (#1202/#1204).
//   - `Shape shape` -- identified by which per-shape sub-vector in BeatMap
//     the row lives in (`shape_gate_circle_beats`, ..., `split_path_triangle_beats`).
//     OnsetMarker has no shape -- only one vector `onset_marker_beats` (#1202/#1204).
//   - `bool has_time_sec` + NULL-column `float time_sec` -- identified by which
//     of the parallel `*_beats` (indexed) / `*_beats_timed` (authored) vector
//     the row lives in (#1533 Site #4, Fabian Principle 3).
//
// For ShapeGate rows `lane` is positional only; for SplitPath rows `lane`
// is the required dodge lane; for OnsetMarker rows `lane` is unused.
struct BeatEntry {
    int    beat_index = 0;
    int8_t lane       = 1;
};

// ── Beat Entry, authored-onset variant ─────────────────────────────
// Row table for entries whose timing source is an explicit authored
// `time_sec` (typically an aubio onset timestamp). Membership of a row
// in any `*_beats_timed` vector IS the "has authored time_sec" precondition
// — there is no in-row optionality flag and `time_sec` is always meaningful.
//
// Untimed (beat-index-only) siblings live in the parallel `*_beats` vectors
// and resolve their arrival time from `BeatMap::beat_times[beat_index]`.
struct BeatEntryTimed {
    int    beat_index = 0;
    int8_t lane       = 1;
    float  time_sec   = 0.0f;
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
// The chart is normalized per Fabian's relational mechanic (#1204, #1533):
// each former `ObstacleKind` and each former `Shape` value gets its own
// table (vector); and each former `bool has_time_sec` optionality on the
// row is split into parallel indexed (`*_beats`) / authored-onset
// (`*_beats_timed`) tables whose membership IS the timing-source
// precondition. Cross-table ordering (e.g. monotonic beat indices) is
// validated by merging the per-(kind, shape, timing-source) vectors in
// beat-index order; intra-table rules walk only the relevant vector.
// Each vector is independently `stable_sort`-ed after parsing
// (indexed bins by `beat_index`; timed bins by `(beat_index, time_sec)`).
//
// Shape::Hexagon is not a valid required shape for shape_gate / split_path
// (`current_required_shape` returns Hexagon as a sentinel meaning "no
// required shape"); the loader rejects shape_gate / split_path rows whose
// shape parses as Hexagon, so no `*_hexagon_beats[_timed]` vector exists.
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

    std::vector<BeatEntryTimed> shape_gate_circle_beats_timed;
    std::vector<BeatEntryTimed> shape_gate_square_beats_timed;
    std::vector<BeatEntryTimed> shape_gate_triangle_beats_timed;
    std::vector<BeatEntryTimed> split_path_circle_beats_timed;
    std::vector<BeatEntryTimed> split_path_square_beats_timed;
    std::vector<BeatEntryTimed> split_path_triangle_beats_timed;
    std::vector<BeatEntryTimed> onset_marker_beats_timed;

    BeatMap()                          = default;
    BeatMap(const BeatMap&)            = delete;
    BeatMap& operator=(const BeatMap&) = delete;
    BeatMap(BeatMap&&)                 = default;
    BeatMap& operator=(BeatMap&&)      = default;
};

// Total scoring-eligible (ShapeGate + SplitPath) beat count across the
// six per-(kind, shape) indexed vectors and their six authored-onset
// siblings. OnsetMarker beats are non-scorable cues and are excluded.
inline size_t beat_map_required_count(const BeatMap& m) noexcept {
    return m.shape_gate_circle_beats.size()
         + m.shape_gate_square_beats.size()
         + m.shape_gate_triangle_beats.size()
         + m.split_path_circle_beats.size()
         + m.split_path_square_beats.size()
         + m.split_path_triangle_beats.size()
         + m.shape_gate_circle_beats_timed.size()
         + m.shape_gate_square_beats_timed.size()
         + m.shape_gate_triangle_beats_timed.size()
         + m.split_path_circle_beats_timed.size()
         + m.split_path_square_beats_timed.size()
         + m.split_path_triangle_beats_timed.size();
}

// Total ShapeGate beat count across the three per-shape indexed vectors
// and their three authored-onset siblings.
inline size_t shape_gate_count(const BeatMap& m) noexcept {
    return m.shape_gate_circle_beats.size()
         + m.shape_gate_square_beats.size()
         + m.shape_gate_triangle_beats.size()
         + m.shape_gate_circle_beats_timed.size()
         + m.shape_gate_square_beats_timed.size()
         + m.shape_gate_triangle_beats_timed.size();
}

// Total SplitPath beat count across the three per-shape indexed vectors
// and their three authored-onset siblings.
inline size_t split_path_count(const BeatMap& m) noexcept {
    return m.split_path_circle_beats.size()
         + m.split_path_square_beats.size()
         + m.split_path_triangle_beats.size()
         + m.split_path_circle_beats_timed.size()
         + m.split_path_square_beats_timed.size()
         + m.split_path_triangle_beats_timed.size();
}

// Total beat count across all fourteen per-(kind, shape, timing-source) vectors.
inline size_t beat_map_total_count(const BeatMap& m) noexcept {
    return beat_map_required_count(m)
         + m.onset_marker_beats.size()
         + m.onset_marker_beats_timed.size();
}

// True iff the beat map has zero authored beats across all fourteen vectors.
inline bool beat_map_empty(const BeatMap& m) noexcept {
    return beat_map_total_count(m) == 0;
}
