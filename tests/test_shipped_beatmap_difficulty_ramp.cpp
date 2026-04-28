// Regression tests for GitHub issues #135 and #217.
//
// Validates difficulty ramp quality in every shipped beatmap:
//
//   EASY  — shape variety
//     • All 3 shapes (Circle, Square, Triangle) must appear at least once.
//     • No single shape > 65 % of all ShapeGate obstacles.
//
//   MEDIUM — LanePush introduction ramp
//     • LanePush (Left + Right combined) must be ≥ 5 % of total obstacles
//       so the mechanic is actually taught.
//     • LanePush must be ≤ 25 % of total to avoid the readability cliff.
//     • No more than 3 consecutive LanePush obstacles (readability ramp).
//     • First LanePush arrival time must be ≥ 30 s so players learn shape
//       gates before the mechanic is introduced. (#217)
//
// All checks use FAIL_CHECK (non-aborting) so every offending song is
// reported in a single run.  The test is tagged [difficulty_ramp][issue135].
//
// Coexistence:
//   - Does not conflict with [low_high_bar] tests (#125): easy/medium have no
//     LowBar/HighBar; the shape variety check only counts ShapeGate obstacles.
//   - Does not conflict with [shipped_beatmaps][issue134]: shape-gap violations
//     are a separate rule checked there; this file counts kind distribution only.
//
// CWD when run via CTest is the build/ directory which has a mirrored
// content/beatmaps/ copy via CMake POST_BUILD commands.

#include <catch2/catch_test_macros.hpp>
#include "util/beat_map_loader.h"
#include "components/beat_map.h"
#include "components/obstacle.h"
#include "components/player.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── Thresholds ────────────────────────────────────────────────────────────

static constexpr int   EASY_MIN_DISTINCT_SHAPES        = 3;
static constexpr float EASY_MAX_DOMINANT_SHAPE_PCT      = 65.0f;

static constexpr float MEDIUM_MIN_LANE_PUSH_PCT         = 5.0f;
static constexpr float MEDIUM_MAX_LANE_PUSH_PCT         = 25.0f;
static constexpr int   MEDIUM_MAX_CONSEC_LANE_PUSH      = 3;
static constexpr float MEDIUM_MIN_FIRST_LANEPUSH_SEC    = 30.0f;

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

static bool is_lane_push(ObstacleKind k) {
    return k == ObstacleKind::LanePushLeft || k == ObstacleKind::LanePushRight;
}

// ── Guard: content directory must be reachable ────────────────────────────

TEST_CASE("difficulty ramp: content directory is reachable from test CWD",
          "[difficulty_ramp][issue135]") {
    REQUIRE(fs::is_directory("content/beatmaps"));
    REQUIRE_FALSE(find_shipped_beatmaps().empty());
}

// ── Easy: shape_gate only — no lane_push, low_bar, or high_bar ───────────
//
// Contract from #125: easy difficulty = shape_gate exclusively.
// This test is the regression guard Kujan required before merging #135.

TEST_CASE("difficulty ramp: easy contains no lane_push, low_bar, or high_bar obstacles",
          "[difficulty_ramp][issue135][easy][shape_gate_only]") {
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        if (!load_beat_map(path, map, errors, "easy")) continue;

        for (const auto& beat : map.beats) {
            const auto k = beat.kind;
            if (k == ObstacleKind::LanePushLeft
                || k == ObstacleKind::LanePushRight
                || k == ObstacleKind::LowBar
                || k == ObstacleKind::HighBar) {
                FAIL_CHECK("easy shape_gate_only: " << path
                           << " contains non-shape_gate obstacle kind="
                           << static_cast<int>(k)
                           << " (lane_push/low_bar/high_bar are hard/medium only per #125)");
            }
        }
    }
}

// ── Easy: shape variety ───────────────────────────────────────────────────

TEST_CASE("difficulty ramp: easy uses all 3 shapes and no single shape dominates",
          "[difficulty_ramp][issue135][easy]") {
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        if (!load_beat_map(path, map, errors, "easy")) continue;

        // Count shapes across ShapeGate obstacles only.
        int shape_counts[3] = {0, 0, 0};   // Circle=0, Square=1, Triangle=2
        int sg_total = 0;

        for (const auto& beat : map.beats) {
            if (beat.kind != ObstacleKind::ShapeGate) continue;
            ++sg_total;
            const auto idx = static_cast<int>(beat.shape);
            if (idx >= 0 && idx < 3) ++shape_counts[idx];
        }

        if (sg_total == 0) continue;  // empty difficulty — skip

        // All 3 shapes must appear at least once.
        int distinct = 0;
        for (int c : shape_counts) {
            if (c > 0) ++distinct;
        }
        if (distinct < EASY_MIN_DISTINCT_SHAPES) {
            FAIL_CHECK("easy variety: " << path
                       << " only uses " << distinct << " distinct shape(s) "
                       << "(need >=" << EASY_MIN_DISTINCT_SHAPES << ")"
                       << "  circle=" << shape_counts[0]
                       << " square=" << shape_counts[1]
                       << " triangle=" << shape_counts[2]);
        }

        // No single shape should dominate.
        static const char* kShapeNames[] = {"Circle", "Square", "Triangle"};
        for (int i = 0; i < 3; ++i) {
            const float pct = 100.0f * static_cast<float>(shape_counts[i])
                              / static_cast<float>(sg_total);
            if (pct > EASY_MAX_DOMINANT_SHAPE_PCT) {
                FAIL_CHECK("easy variety: " << path
                           << " shape '" << kShapeNames[i] << "' is "
                           << pct << "% of shape_gates "
                           << "(limit " << EASY_MAX_DOMINANT_SHAPE_PCT << "%)");
            }
        }
    }
}

// ── Medium: LanePush ramp ─────────────────────────────────────────────────

TEST_CASE("difficulty ramp: medium LanePush percentage within [5%, 25%]",
          "[difficulty_ramp][issue135][medium]") {
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        if (!load_beat_map(path, map, errors, "medium")) continue;

        const int total = static_cast<int>(map.beats.size());
        if (total == 0) continue;

        int lp_count = 0;
        for (const auto& beat : map.beats) {
            if (is_lane_push(beat.kind)) ++lp_count;
        }

        const float lp_pct = 100.0f * static_cast<float>(lp_count)
                             / static_cast<float>(total);

        if (lp_pct < MEDIUM_MIN_LANE_PUSH_PCT) {
            FAIL_CHECK("medium ramp: " << path
                       << " LanePush only " << lp_pct << "% of obstacles"
                       << " (need >=" << MEDIUM_MIN_LANE_PUSH_PCT
                       << "% to teach the mechanic)");
        }
        if (lp_pct > MEDIUM_MAX_LANE_PUSH_PCT) {
            FAIL_CHECK("medium ramp: " << path
                       << " LanePush at " << lp_pct << "% of obstacles"
                       << " (limit " << MEDIUM_MAX_LANE_PUSH_PCT
                       << "% to avoid readability cliff)");
        }
    }
}

TEST_CASE("difficulty ramp: medium LanePush max consecutive run <= 3",
          "[difficulty_ramp][issue135][medium]") {
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        if (!load_beat_map(path, map, errors, "medium")) continue;

        int max_consec = 0;
        int cur = 0;
        for (const auto& beat : map.beats) {
            if (is_lane_push(beat.kind)) {
                ++cur;
                if (cur > max_consec) max_consec = cur;
            } else {
                cur = 0;
            }
        }

        if (max_consec > MEDIUM_MAX_CONSEC_LANE_PUSH) {
            FAIL_CHECK("medium ramp: " << path
                       << " has " << max_consec << " consecutive LanePush obstacles"
                       << " (limit " << MEDIUM_MAX_CONSEC_LANE_PUSH
                       << " for readability)");
        }
    }
}

// ── Medium: first LanePush arrival time ≥ 30 s ───────────────────────────
//
// Design contract from #135 (decision doc gap noted as non-blocking):
// LanePush must not appear before 30 seconds so players learn shape gates
// first. Arrival time is computed from bpm and offset in the beatmap JSON:
//     arrival = offset + beat_index * (60.0 / bpm)
//
// Uses the minimum beat_index among all LanePush beats in case the obstacle
// list is not strictly sorted.

TEST_CASE("difficulty ramp: medium first LanePush arrival time >= 30 s",
          "[difficulty_ramp][issue217][medium]") {
    const auto beatmaps = find_shipped_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        BeatMap map;
        std::vector<BeatMapError> errors;
        if (!load_beat_map(path, map, errors, "medium")) continue;

        // Find the beat_index of the earliest LanePush obstacle.
        int first_beat_index = -1;
        for (const auto& beat : map.beats) {
            if (!is_lane_push(beat.kind)) continue;
            if (first_beat_index < 0 || beat.beat_index < first_beat_index)
                first_beat_index = beat.beat_index;
        }

        if (first_beat_index < 0) continue;  // no LanePush in this map — skip

        const float beat_period   = 60.0f / map.bpm;
        const float first_arrival = map.offset + static_cast<float>(first_beat_index) * beat_period;

        if (first_arrival < MEDIUM_MIN_FIRST_LANEPUSH_SEC) {
            FAIL_CHECK("medium start_progress: " << path
                       << " first LanePush (beat_index=" << first_beat_index
                       << ") arrives at " << first_arrival << " s"
                       << " (minimum " << MEDIUM_MIN_FIRST_LANEPUSH_SEC << " s per #135/#217)");
        }
    }
}
