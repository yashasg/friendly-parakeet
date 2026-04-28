// Regression test for GitHub issue #138.
//
// Loads every shipped beatmap (content/beatmaps/*_beatmap.json) for each
// standard difficulty and asserts that no mid-song silent gap (sequence of
// consecutive beats with no obstacles between authored obstacles) exceeds
// per-difficulty limits.
//
// Excessive mid-song gaps create unplayable silent sections. This test catches
// regressions where obstacles are filtered too aggressively.
//
// Issue #138: "Shipped songs contain 56-64 beat silent gaps"
// Root causes fixed:
//   1. clean_gap_monotony was too aggressive (0.40 -> 0.50 threshold)
//   2. SELECT phase was not distributing fills across sparse sections
//
// CWD when running via CTest is the build directory, which has a copy of
// content/ created by CMake POST_BUILD commands.

#include <catch2/catch_test_macros.hpp>
#include "util/beat_map_loader.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>
#include <set>

namespace fs = std::filesystem;

// Maximum silent gap (in beats) per difficulty (issue #138)
static constexpr int MAX_GAP_EASY   = 40;
static constexpr int MAX_GAP_MEDIUM = 32;
static constexpr int MAX_GAP_HARD   = 30;

static int get_max_gap(const std::vector<int>& beats) {
    if (beats.size() <= 1) {
        return 0;
    }
    std::set<int> beat_set(beats.begin(), beats.end());
    std::vector<int> unique_beats(beat_set.begin(), beat_set.end());

    int max_gap = 0;
    for (size_t i = 1; i < unique_beats.size(); ++i) {
        max_gap = std::max(max_gap, unique_beats[i] - unique_beats[i - 1] - 1);
    }
    return max_gap;
}

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

TEST_CASE("shipped beatmaps: no excessive silent gaps (max beat gap validation)",
          "[shipped_beatmaps][regression][issue138]") {
    const char* kDiffs[] = {"easy", "medium", "hard"};
    const int kMaxGaps[] = {MAX_GAP_EASY, MAX_GAP_MEDIUM, MAX_GAP_HARD};

    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        for (size_t diff_idx = 0; diff_idx < 3; ++diff_idx) {
            const char* diff = kDiffs[diff_idx];
            const int max_allowed = kMaxGaps[diff_idx];

            BeatMap map;
            std::vector<BeatMapError> load_errors;

            // load_beat_map returns false only on file-open or JSON-parse failure;
            // a missing difficulty produces a fallback warning (not false).  Skip
            // only on hard I/O / JSON failures.
            if (!load_beat_map(path, map, load_errors, diff)) continue;

            // Extract beat indices
            std::vector<int> beats;
            for (const auto& entry : map.beats) {
                beats.push_back(entry.beat_index);
            }
            std::sort(beats.begin(), beats.end());

            int max_gap = get_max_gap(beats);

            if (max_gap > max_allowed) {
                FAIL_CHECK("max beat gap violation: "
                           << path << " [" << diff << "]"
                           << "  max_gap=" << max_gap
                           << " (limit " << max_allowed << ")");
            }
        }
    }
}
