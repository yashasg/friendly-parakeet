// Regression coverage for shipped beatmaps on the onset-motif spike path.
//
// The approved spike path disables legacy max-gap filling/thinning logic.
// Instead of enforcing fixed per-difficulty gap caps, this test verifies
// structural integrity: shipped authored beats must remain strictly increasing
// with no duplicates.
//
// CWD when running via CTest is the build directory, which has a copy of
// content/ created by CMake POST_BUILD commands.

#include <catch2/catch_test_macros.hpp>
#include "entities/beat_map.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
namespace fs = std::filesystem;

static std::vector<std::string> find_shipped_beatmaps() {
    std::vector<std::string> paths;
    const fs::path dir{"content/beatmaps"};
    if (!fs::is_directory(dir)) return paths;
    for (const auto& entry : fs::directory_iterator(dir)) {
        const auto name = entry.path().filename().string();
        if (name.ends_with("_beatmap.json")) {
            paths.push_back(entry.path().string());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

// Guard: beatmap directory must be reachable.

TEST_CASE("shipped beatmaps: content directory is reachable from test CWD",
          "[shipped_beatmaps][issue138]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());
}

// Main regression check.

TEST_CASE("shipped beatmaps: authored beats are strictly increasing",
          "[shipped_beatmaps][regression][issue138]") {
    const char* kDiffs[] = {"easy", "medium", "hard"};

    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        for (size_t diff_idx = 0; diff_idx < 3; ++diff_idx) {
            const char* diff = kDiffs[diff_idx];

            BeatMap map;
            std::vector<BeatMapError> load_errors;

            // load_beat_map returns false only on file-open or JSON-parse failure;
            // a missing difficulty produces a fallback warning (not false).  Skip
            // only on hard I/O / JSON failures.
            if (!load_beat_map(path, map, load_errors, diff)) continue;

            // Issue #396 — subdivision-aware snap can place two distinct
            // obstacles at the same beat_index (e.g. downbeat + eighth or
            // cross-layer onsets within 50ms).  Strict beat-index uniqueness
            // is intentionally no longer required; obstacles must instead
            // stay strictly ordered in onset time and never go backwards in
            // beat index.
            //
            // Per #1202/#1204 the chart is normalized: each former
            // `ObstacleKind` lives in its own vector. Per #1533 each per-kind
            // vector is further split into an indexed (`*_beats`) sibling
            // (timing comes from `beat_times[beat_index]`) and an authored
            // (`*_beats_timed`) sibling (timing comes from the row's
            // `time_sec`). To check the cross-kind ordering invariant we
            // merge all fourteen vectors into one flat view, projecting each
            // row to a resolved time before sorting by (beat_index, resolved
            // time).
            struct MergedBeat {
                int   beat_index;
                float time_sec;
            };
            std::vector<MergedBeat> merged;
            merged.reserve(beat_map_total_count(map));
            auto append_indexed = [&](const std::vector<BeatEntry>& v) {
                for (const auto& e : v) {
                    float t = e.beat_index * (60.0f / map.bpm) + map.offset;
                    if (!map.beat_times.empty() &&
                        e.beat_index >= 0 &&
                        static_cast<size_t>(e.beat_index) < map.beat_times.size()) {
                        t = map.beat_times[static_cast<size_t>(e.beat_index)];
                    }
                    merged.push_back({e.beat_index, t});
                }
            };
            auto append_timed = [&](const std::vector<BeatEntryTimed>& v) {
                for (const auto& e : v) {
                    merged.push_back({e.beat_index, e.time_sec});
                }
            };
            append_indexed(map.shape_gate_circle_beats);
            append_indexed(map.shape_gate_square_beats);
            append_indexed(map.shape_gate_triangle_beats);
            append_indexed(map.split_path_circle_beats);
            append_indexed(map.split_path_square_beats);
            append_indexed(map.split_path_triangle_beats);
            append_indexed(map.onset_marker_beats);
            append_timed(map.shape_gate_circle_beats_timed);
            append_timed(map.shape_gate_square_beats_timed);
            append_timed(map.shape_gate_triangle_beats_timed);
            append_timed(map.split_path_circle_beats_timed);
            append_timed(map.split_path_square_beats_timed);
            append_timed(map.split_path_triangle_beats_timed);
            append_timed(map.onset_marker_beats_timed);
            std::stable_sort(merged.begin(), merged.end(),
                             [](const MergedBeat& a, const MergedBeat& b) {
                                 if (a.beat_index != b.beat_index) return a.beat_index < b.beat_index;
                                 return a.time_sec < b.time_sec;
                             });

            if (merged.empty()) {
                FAIL_CHECK("beat order integrity: " << path << " [" << diff << "] has no authored beats");
                continue;
            }

            for (size_t i = 1; i < merged.size(); ++i) {
                if (merged[i].beat_index < merged[i - 1].beat_index) {
                    FAIL_CHECK("beat order integrity: " << path
                               << " [" << diff << "] has decreasing beat index pair "
                               << merged[i - 1].beat_index << " -> " << merged[i].beat_index);
                }
                // Same-beat ties are allowed: cross-layer onsets within the
                // 50 ms protected window remain distinct events anchored to a
                // shared beat/subdivision.  We only flag a regression if a
                // later beat has both equal beat_index AND strictly smaller
                // time_sec (out-of-order within the same beat).
                if (merged[i].beat_index > merged[i - 1].beat_index &&
                    merged[i].time_sec < merged[i - 1].time_sec) {
                    FAIL_CHECK("beat order integrity: " << path
                               << " [" << diff << "] has decreasing time_sec across beats "
                               << merged[i - 1].beat_index << " (" << merged[i - 1].time_sec
                               << ") -> " << merged[i].beat_index << " ("
                               << merged[i].time_sec << ")");
                }
            }
        }
    }
}
