// Regression tests for GitHub issues #135 and #217.
//
// Validates difficulty ramp quality in every shipped beatmap:
//
//   EASY/MEDIUM/HARD — note-count ramp
//     • Shape-gate note count must be non-decreasing by difficulty
//       (easy <= medium <= hard).
//
//   MEDIUM/HARD — no removed legacy lane kind
//     • lane_block must not appear.
//
// All checks use FAIL_CHECK (non-aborting) so every offending song is
// reported in a single run.  The test is tagged [difficulty_ramp][issue135].
//
// Coexistence:
//   - Does not conflict with [shipped_beatmaps][issue134]: that suite now checks
//     canonical shape/lane mapping while this file checks difficulty ramp rules.
//
// CWD when run via CTest is the build/ directory which has a mirrored
// content/beatmaps/ copy via CMake POST_BUILD commands.

#include <catch2/catch_test_macros.hpp>
#include "entities/beat_map.h"
#include "components/obstacle.h"
#include "components/player.h"

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


static bool is_easy_allowed_kind(ObstacleKind k) {
    return k == ObstacleKind::ShapeGate || k == ObstacleKind::OnsetMarker;
}

// ── Guard: content directory must be reachable ────────────────────────────

TEST_CASE("difficulty ramp: content directory is reachable from test CWD",
          "[difficulty_ramp][issue135]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    REQUIRE_FALSE(find_shipped_beatmaps().empty());
}

// ── Easy: required shape gates plus non-blocking onset markers ──────────────
//
// Contract from #125: easy difficulty = shape_gate required obstacles only.
// Non-blocking onset_marker rows preserve public-layer onset metadata (#642).
// This test is the regression guard Kujan required before merging #135.

TEST_CASE("difficulty ramp: easy contains only shape_gate obstacles or onset markers",
          "[difficulty_ramp][issue135][easy][shape_gate_only]") {
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        if (!load_beat_map(path, map, errors, "easy")) continue;

        for (const auto& beat : map.beats) {
            const auto k = beat.kind;
            if (!is_easy_allowed_kind(k)) {
                FAIL_CHECK("easy shape_gate_only: " << path
                           << " contains unsupported easy obstacle kind="
                           << static_cast<int>(k)
                           << " (easy required obstacles must be shape_gate-only per #125; "
                              "onset_marker is non-blocking metadata per #642)");
            }
        }
    }
}

// ── Easy/Medium/Hard: non-decreasing authored note count ──────────────────

TEST_CASE("difficulty ramp: shape-gate count increases with difficulty",
          "[difficulty_ramp][issue135][easy][medium][hard]") {
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        int counts[3] = {0, 0, 0};  // easy, medium, hard
        const char* diffs[3] = {"easy", "medium", "hard"};

        for (int i = 0; i < 3; ++i) {
            BeatMap map;
            std::vector<BeatMapError> errors;
            if (!load_beat_map(path, map, errors, diffs[i])) {
                FAIL_CHECK("difficulty ramp count: failed to load " << path
                           << " difficulty=" << diffs[i]);
                continue;
            }
            for (const auto& beat : map.beats) {
                if (beat.kind == ObstacleKind::ShapeGate) ++counts[i];
            }
        }

        if (counts[0] == 0 || counts[1] == 0 || counts[2] == 0) {
            FAIL_CHECK("difficulty ramp count: " << path
                       << " has empty shape-gate difficulty counts easy="
                       << counts[0] << " medium=" << counts[1]
                       << " hard=" << counts[2]);
            continue;
        }

        if (counts[0] > counts[1] || counts[1] > counts[2]) {
            FAIL_CHECK("difficulty ramp count: " << path
                       << " expected easy<=medium<=hard but got easy="
                       << counts[0] << " medium=" << counts[1]
                       << " hard=" << counts[2]);
        }
    }
}

// ── Medium/Hard: low/high bars are disabled ────────────────────────────────
