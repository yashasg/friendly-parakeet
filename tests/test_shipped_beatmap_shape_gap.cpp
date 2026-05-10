// Regression coverage for shipped beatmaps on the onset-motif spike path.
//
// The spike intentionally preserves onset-class mapping even when pockets
// contain tightly spaced shape changes.  We therefore gate shipped content on
// canonical lane/shape pairing rather than a fixed min-shape-gap window.
//
// CWD when running via CTest is the build directory, which has a copy of
// content/ created by CMake POST_BUILD commands.  The same relative path
// ("content/beatmaps/...") therefore works both when running the binary
// directly from build/ and through `ctest`.
//

#include <catch2/catch_test_macros.hpp>
#include "util/beat_map_loader.h"
#include "util/shape_lane_mapping.h"

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

// ── Guard: beatmap directory must be reachable ─────────────────────────────

TEST_CASE("shipped beatmaps: content directory is reachable from test CWD",
          "[shipped_beatmaps][issue134]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());
}

// ── Main regression check ──────────────────────────────────────────────────

TEST_CASE("shipped beatmaps: shape gates keep canonical lane/shape mapping",
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

            for (const auto& beat : map.beats) {
                if (beat.kind != ObstacleKind::ShapeGate) continue;
                const int expected_lane = lane_for_shape(beat.shape);
                if (expected_lane < 0) {
                    FAIL_CHECK("canonical mapping: " << path << " [" << diff
                               << "] beat " << beat.beat_index
                               << " has unknown shape enum="
                               << static_cast<int>(beat.shape));
                    continue;
                }
                if (beat.lane != expected_lane) {
                    FAIL_CHECK("canonical mapping: " << path << " [" << diff
                               << "] beat " << beat.beat_index
                               << " shape enum=" << static_cast<int>(beat.shape)
                               << " expected lane " << expected_lane
                               << " but found lane " << static_cast<int>(beat.lane));
                }
            }
        }
    }
}
