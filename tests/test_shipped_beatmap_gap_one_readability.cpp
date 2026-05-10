// Regression tests for GitHub issue #141 on the onset-motif spike path.
//
// The approved onset path disables legacy gap=1 readability enforcement.
// We keep a lightweight guard: if consecutive beats are one beat apart,
// obstacles must still be shape gates.

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

TEST_CASE("gap=1 readability: content directory is reachable",
          "[shipped_beatmaps][issue141]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    REQUIRE_FALSE(find_shipped_beatmaps().empty());
}

TEST_CASE("gap=1 readability: adjacent authored beats remain shape gates",
          "[shipped_beatmaps][regression][issue141]") {
    constexpr const char* kDifficulties[] = {"easy", "medium", "hard"};

    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        for (const char* difficulty : kDifficulties) {
            BeatMap map;
            std::vector<BeatMapError> errors;
            if (!load_beat_map(path, map, errors, difficulty)) continue;

            const auto& beats = map.beats;
            if (beats.size() <= 1) continue;

            for (size_t index = 1; index < beats.size(); ++index) {
                const auto& left = beats[index - 1];
                const auto& right = beats[index];
                const int gap = right.beat_index - left.beat_index;

                if (gap != 1) continue;
                if (left.kind != ObstacleKind::ShapeGate || right.kind != ObstacleKind::ShapeGate) {
                    FAIL_CHECK(path << " [" << difficulty
                               << "] gap=1 contains non-shape-gate obstacle at beat "
                               << left.beat_index);
                }
            }
        }
    }
}
