// Regression tests for GitHub issue #142.
//
// Validates medium shape/lane distribution in shipped beatmaps using the
// runtime beatmap loader path.

#include <catch2/catch_test_macros.hpp>
#include "entities/beat_map.h"
#include "components/obstacle.h"
#include "util/shape_lane_mapping.h"

#include <algorithm>
#include <array>
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

// Per-shape motif lane column (issue #1202/#1204). The former
// `switch (shape)` is replaced by a constexpr int column indexed by
// `shape_index(shape)`; Hexagon's slot is -1 (no motif lane).
static constexpr std::array<int, kShapeCount> kMotifLaneForShape{
    0,   // Circle
    1,   // Square
    2,   // Triangle
    -1,  // Hexagon — not lane-mapped
};

static int expected_lane_for_shape(Shape shape) {
    const int idx = shape_index(shape);
    if (idx < 0 || idx >= kShapeCount) return -1;
    return kMotifLaneForShape[static_cast<size_t>(idx)];
}

TEST_CASE("medium balance: content directory is reachable",
          "[shipped_beatmaps][issue142]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    REQUIRE_FALSE(find_shipped_beatmaps().empty());
}

TEST_CASE("medium balance: shipped beatmaps keep motif lane mapping",
          "[shipped_beatmaps][regression][issue142]") {
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        if (!load_beat_map(path, map, errors, "medium")) continue;

        // Per #1202/#1204: shape-gate entries live in their own per-kind vector.
        int total_shape_gates = 0;

        for (const auto& beat : map.shape_gate_beats) {
            ++total_shape_gates;
            const int expected_lane = expected_lane_for_shape(beat.shape);
            if (expected_lane < 0) {
                FAIL_CHECK("medium balance: " << path
                           << " beat " << beat.beat_index
                           << " has unknown shape enum="
                           << static_cast<int>(beat.shape));
                continue;
            }
            if (beat.lane != expected_lane) {
                FAIL_CHECK("medium balance: " << path
                           << " beat " << beat.beat_index
                           << " shape enum=" << static_cast<int>(beat.shape)
                           << " expected lane " << expected_lane
                           << " but found lane " << static_cast<int>(beat.lane));
            }
        }
        REQUIRE(total_shape_gates > 0);
    }
}
