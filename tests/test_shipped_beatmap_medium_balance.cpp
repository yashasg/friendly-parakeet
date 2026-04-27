// Regression tests for GitHub issue #142.
//
// Validates medium shape/lane distribution in shipped beatmaps using the
// runtime beatmap loader path.

#include <catch2/catch_test_macros.hpp>
#include "components/beat_map.h"
#include "components/obstacle.h"
#include "systems/beat_map_loader.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct DistributionRange {
    float min_pct;
    float max_pct;
};

static constexpr std::array<DistributionRange, 3> MEDIUM_SHAPE_RANGES = {{
    {10.0f, 20.0f}, // Circle / lane 0
    {45.0f, 60.0f}, // Square / lane 1
    {25.0f, 45.0f}, // Triangle / lane 2
}};

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

static int shape_index(Shape shape) {
    return static_cast<int>(shape);
}

TEST_CASE("medium balance: content directory is reachable",
          "[shipped_beatmaps][issue142]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    REQUIRE_FALSE(find_shipped_beatmaps().empty());
}

TEST_CASE("medium balance: shipped beatmaps have balanced shape-gate lanes",
          "[shipped_beatmaps][regression][issue142]") {
    static constexpr std::array<const char*, 3> kShapeNames = {
        "Circle", "Square", "Triangle"
    };

    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        if (!load_beat_map(path, map, errors, "medium")) continue;

        int shape_counts[3] = {0, 0, 0};
        int lane_counts[3] = {0, 0, 0};
        int total_shape_gates = 0;

        for (const auto& beat : map.beats) {
            if (beat.kind != ObstacleKind::ShapeGate) continue;

            ++total_shape_gates;
            const int shape = shape_index(beat.shape);
            if (shape >= 0 && shape < 3) ++shape_counts[shape];
            if (beat.lane >= 0 && beat.lane < 3) ++lane_counts[beat.lane];
        }

        REQUIRE(total_shape_gates > 0);

        for (int i = 0; i < 3; ++i) {
            const float shape_pct = 100.0f * static_cast<float>(shape_counts[i])
                / static_cast<float>(total_shape_gates);
            const float lane_pct = 100.0f * static_cast<float>(lane_counts[i])
                / static_cast<float>(total_shape_gates);
            const auto range = MEDIUM_SHAPE_RANGES[static_cast<size_t>(i)];

            if (shape_pct < range.min_pct || shape_pct > range.max_pct) {
                FAIL_CHECK("medium balance: " << path << " shape " << kShapeNames[i]
                           << " is " << shape_pct << "% of shape_gates"
                           << " (target " << range.min_pct << "%-"
                           << range.max_pct << "%)");
            }

            if (lane_pct < range.min_pct || lane_pct > range.max_pct) {
                FAIL_CHECK("medium balance: " << path << " lane " << i
                           << " is " << lane_pct << "% of shape_gates"
                           << " (target " << range.min_pct << "%-"
                           << range.max_pct << "%)");
            }
        }
    }
}
