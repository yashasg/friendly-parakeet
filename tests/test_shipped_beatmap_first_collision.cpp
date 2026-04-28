// Regression test for GitHub issue #175.
//
// Asserts that the first authored obstacle in every shipped beatmap clears
// the per-difficulty reaction floor:
//
//     first_collision_sec = offset + first.beat * (60 / bpm)
//
//     easy   ≥ 4.0 s
//     medium ≥ 2.5 s
//     hard   ≥ 2.0 s
//
// Spec: design-docs/rhythm-spec.md "First-collision floor (per difficulty)".
// Generator floor enforced in tools/level_designer.py
// (`enforce_first_collision_floor`).
//
// Pre-#175 evidence: hard difficulty shipped first.beat=2 on every song,
// yielding 0.75–1.00 s reaction windows on stomper/drama and 1.22 s on
// mental_corruption — under one second of game time for new players to read
// the field, identify the lane and shape, and morph.
//
// CWD when run via CTest is the build/ directory which has a mirrored
// content/beatmaps/ copy via CMake POST_BUILD commands.

#include <catch2/catch_test_macros.hpp>
#include "util/beat_map_loader.h"
#include "components/beat_map.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── Floors (seconds) ──────────────────────────────────────────────────────

static constexpr float FIRST_COLLISION_MIN_EASY   = 4.0f;
static constexpr float FIRST_COLLISION_MIN_MEDIUM = 2.5f;
static constexpr float FIRST_COLLISION_MIN_HARD   = 2.0f;

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

TEST_CASE("first collision: every shipped difficulty clears its reaction floor",
          "[first_collision][issue175][regression]") {
    const char* kDiffs[]   = {"easy", "medium", "hard"};
    const float kFloors[]  = {
        FIRST_COLLISION_MIN_EASY,
        FIRST_COLLISION_MIN_MEDIUM,
        FIRST_COLLISION_MIN_HARD,
    };

    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        for (size_t i = 0; i < 3; ++i) {
            const char* diff   = kDiffs[i];
            const float floor  = kFloors[i];

            BeatMap map;
            std::vector<BeatMapError> load_errors;
            if (!load_beat_map(path, map, load_errors, diff)) continue;
            if (map.beats.empty()) continue;

            // Find the smallest beat_index — entries are non-decreasing but
            // we don't rely on it.
            int first_beat = map.beats.front().beat_index;
            for (const auto& b : map.beats) {
                if (b.beat_index < first_beat) first_beat = b.beat_index;
            }

            const float beat_period = (map.bpm > 0.0f) ? (60.0f / map.bpm) : 0.5f;
            const float first_collision = map.offset + static_cast<float>(first_beat) * beat_period;

            if (first_collision + 1e-3f < floor) {
                FAIL_CHECK("first-collision floor violation: " << path
                           << " [" << diff << "]"
                           << "  first.beat=" << first_beat
                           << "  bpm=" << map.bpm
                           << "  offset=" << map.offset
                           << "  first_collision=" << first_collision << "s"
                           << "  (floor " << floor << "s)");
            }
        }
    }
}
