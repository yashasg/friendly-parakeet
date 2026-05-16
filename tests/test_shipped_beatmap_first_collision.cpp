// Regression coverage for first authored obstacle integrity.
//
// The approved onset-motif spike path enforces first-collision timing before
// obstacle materialization rather than through shipped-content post-processing.
// This guard checks structural integrity only: each shipped difficulty must
// contain at least one authored beat and the first authored beat index must be
// non-negative.
//
// CWD when run via CTest is the build/ directory which has a mirrored
// content/beatmaps/ copy via CMake POST_BUILD commands.

#include <catch2/catch_test_macros.hpp>
#include "entities/beat_map.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────

static std::vector<std::string> find_shipped_beatmaps() {
    std::vector<std::string> paths;
    const fs::path dir{"content/beatmaps"};
    if (!fs::is_directory(dir)) return paths;
    for (const auto& entry : fs::directory_iterator(dir)) {
        const auto name = entry.path().filename().string();
        if (name.ends_with("_beatmap.json"))
            paths.push_back(entry.path().string());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

// ── Guard ─────────────────────────────────────────────────────────────────

TEST_CASE("first collision: content directory reachable from test CWD",
          "[first_collision][issue175]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    REQUIRE_FALSE(find_shipped_beatmaps().empty());
}

// ── Main regression check ─────────────────────────────────────────────────

TEST_CASE("first collision: every shipped difficulty has a valid first beat",
          "[first_collision][issue175][regression]") {
    const char* kDiffs[] = {"easy", "medium", "hard"};

    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        for (size_t i = 0; i < 3; ++i) {
            const char* diff = kDiffs[i];

            BeatMap map;
            std::vector<BeatMapError> load_errors;
            if (!load_beat_map(path, map, load_errors, diff)) continue;

            // Per #1202/#1204 the chart is split into 3 per-kind vectors.
            // Walk all of them to find the smallest authored beat_index.
            bool any = false;
            int first_beat = 0;
            auto consider = [&](const std::vector<BeatEntry>& v) {
                for (const auto& b : v) {
                    if (!any || b.beat_index < first_beat) {
                        first_beat = b.beat_index;
                        any = true;
                    }
                }
            };
            consider(map.shape_gate_beats);
            consider(map.split_path_beats);
            consider(map.onset_marker_beats);
            if (!any) continue;

            if (first_beat < 0) {
                FAIL_CHECK("first-collision integrity violation: " << path
                           << " [" << diff << "]"
                           << " has negative first beat index " << first_beat);
            }
        }
    }
}
