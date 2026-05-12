// Regression tests for GitHub issue #142.
//
// Validates medium shape/lane distribution in shipped beatmaps using the
// runtime beatmap loader path.

#include <catch2/catch_test_macros.hpp>
#include "components/beat_map.h"
#include "components/obstacle.h"
#include "util/beat_map_loader.h"

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

static int expected_lane_for_shape(Shape shape) {
    switch (shape) {
        case Shape::Circle: return 0;
        case Shape::Square: return 1;
        case Shape::Triangle: return 2;
        case Shape::Hexagon: return -1;
    }
    return -1;
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

        int total_shape_gates = 0;

        for (const auto& beat : map.beats) {
            if (beat.kind != ObstacleKind::ShapeGate) continue;
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
