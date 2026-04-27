// Regression test for GitHub issue #134.
//
// Loads every shipped beatmap (content/beatmaps/*_beatmap.json) for each
// standard difficulty and asserts that no different-shape gate transition
// violates min_shape_change_gap = 3.
//
// CWD when running via CTest is the build directory, which has a copy of
// content/ created by CMake POST_BUILD commands.  The same relative path
// ("content/beatmaps/...") therefore works both when running the binary
// directly from build/ and through `ctest`.
//
// Does NOT regress #125 LowBar/HighBar coverage — those kinds are
// explicitly excluded from the shape-change check in validate_beat_map.

#include <catch2/catch_test_macros.hpp>
#include "systems/beat_map_loader.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Substring present in every shape-gap error message produced by validate_beat_map.
static constexpr const char* SHAPE_GAP_MSG = "Different-shape gates must be >=";

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

// ── Guard: beatmap directory must be reachable ─────────────────────────────

TEST_CASE("shipped beatmaps: content directory is reachable from test CWD",
          "[shipped_beatmaps][issue134]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());
}

// ── Main regression check ──────────────────────────────────────────────────

TEST_CASE("shipped beatmaps: no min_shape_change_gap violations in any difficulty",
          "[shipped_beatmaps][regression][issue134]") {
    static const char* kDiffs[] = {"easy", "medium", "hard"};

    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        for (const char* diff : kDiffs) {
            BeatMap map;
            std::vector<BeatMapError> load_errors;

            // load_beat_map returns false only on file-open or JSON-parse failure;
            // a missing difficulty produces a fallback warning (not false).  Skip
            // only on hard I/O / JSON failures.
            if (!load_beat_map(path, map, load_errors, diff)) continue;

            std::vector<BeatMapError> val_errors;
            validate_beat_map(map, val_errors);

            for (const auto& e : val_errors) {
                if (e.message.find(SHAPE_GAP_MSG) != std::string::npos) {
                    // FAIL_CHECK reports the violation and continues so every
                    // offending beat is surfaced in a single test run.
                    FAIL_CHECK("shape-gap violation: "
                               << path << " [" << diff << "]"
                               << "  beat " << e.beat_index
                               << "  — " << e.message);
                }
            }
        }
    }
}
