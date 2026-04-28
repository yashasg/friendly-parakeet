// Regression tests for GitHub issue #141.
//
// Validates readability rules for one-beat obstacle gaps in shipped beatmaps.

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

static constexpr float GAP_ONE_MEDIUM_START_PROGRESS = 0.30f;
static constexpr int GAP_ONE_HARD_MIN_BEAT = 11;
static constexpr int GAP_ONE_MEDIUM_MAX_RUN = 1;
static constexpr int GAP_ONE_HARD_MAX_RUN = 2;

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

static bool is_readable_family(const BeatEntry& left, const BeatEntry& right) {
    return left.kind == ObstacleKind::ShapeGate
        && right.kind == ObstacleKind::ShapeGate
        && left.shape == right.shape
        && left.lane == right.lane;
}

static bool is_unreadable_kind(ObstacleKind kind) {
    return kind == ObstacleKind::LanePushLeft
        || kind == ObstacleKind::LanePushRight
        || kind == ObstacleKind::LowBar
        || kind == ObstacleKind::HighBar;
}

static bool has_readable_neighbors(const std::vector<BeatEntry>& beats, size_t left_index) {
    const auto& left = beats[left_index];
    const auto& right = beats[left_index + 1];

    if (left_index > 0) {
        const auto& previous = beats[left_index - 1];
        if (left.beat_index - previous.beat_index <= 2 && is_unreadable_kind(previous.kind)) {
            return false;
        }
    }

    const size_t following_index = left_index + 2;
    if (following_index < beats.size()) {
        const auto& following = beats[following_index];
        if (following.beat_index - right.beat_index <= 2 && is_unreadable_kind(following.kind)) {
            return false;
        }
    }

    return true;
}

static int max_authored_beat(const std::vector<BeatEntry>& beats) {
    int last = 1;
    for (const auto& beat : beats) {
        last = std::max(last, beat.beat_index);
    }
    return last;
}

TEST_CASE("gap=1 readability: content directory is reachable",
          "[shipped_beatmaps][issue141]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    REQUIRE_FALSE(find_shipped_beatmaps().empty());
}

TEST_CASE("gap=1 readability: shipped beatmaps obey difficulty policy",
          "[shipped_beatmaps][regression][issue141]") {
    constexpr std::array<const char*, 3> kDifficulties = {"easy", "medium", "hard"};

    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        for (const char* difficulty : kDifficulties) {
            BeatMap map;
            std::vector<BeatMapError> errors;
            if (!load_beat_map(path, map, errors, difficulty)) continue;

            const auto& beats = map.beats;
            if (beats.size() <= 1) continue;

            const int last_beat = max_authored_beat(beats);
            int gap_one_run = 0;

            for (size_t index = 1; index < beats.size(); ++index) {
                const auto& left = beats[index - 1];
                const auto& right = beats[index];
                const int gap = right.beat_index - left.beat_index;

                if (gap != 1) {
                    gap_one_run = 0;
                    continue;
                }

                if (std::string{difficulty} == "easy") {
                    FAIL_CHECK(path << " [easy] gap=1 at beat " << left.beat_index);
                } else if (std::string{difficulty} == "medium") {
                    const float progress = static_cast<float>(left.beat_index)
                        / static_cast<float>(last_beat);
                    if (progress <= GAP_ONE_MEDIUM_START_PROGRESS) {
                        FAIL_CHECK(path << " [medium] gap=1 before teaching threshold at beat "
                                   << left.beat_index << " progress=" << progress);
                    }
                    if (gap_one_run >= GAP_ONE_MEDIUM_MAX_RUN) {
                        FAIL_CHECK(path << " [medium] gap=1 run exceeds "
                                   << GAP_ONE_MEDIUM_MAX_RUN << " at beat " << left.beat_index);
                    }
                } else {
                    if (left.beat_index < GAP_ONE_HARD_MIN_BEAT) {
                        FAIL_CHECK(path << " [hard] gap=1 before intro guard at beat "
                                   << left.beat_index);
                    }
                    if (gap_one_run >= GAP_ONE_HARD_MAX_RUN) {
                        FAIL_CHECK(path << " [hard] gap=1 run exceeds "
                                   << GAP_ONE_HARD_MAX_RUN << " at beat " << left.beat_index);
                    }
                }

                if (!is_readable_family(left, right)) {
                    FAIL_CHECK(path << " [" << difficulty
                               << "] gap=1 is not identical shape_gate family at beat "
                               << left.beat_index);
                }

                if (!has_readable_neighbors(beats, index - 1)) {
                    FAIL_CHECK(path << " [" << difficulty
                               << "] gap=1 too close to LanePush/bar at beat "
                               << left.beat_index);
                }

                ++gap_one_run;
            }
        }
    }
}
