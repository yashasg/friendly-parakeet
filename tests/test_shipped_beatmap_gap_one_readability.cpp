// Regression tests for GitHub issue #141 on the onset-motif spike path.
//
// The approved onset path disables legacy gap=1 readability enforcement.
// We keep a lightweight guard: if consecutive required obstacles are one beat
// apart, they must still be shape gates. Non-blocking onset_marker rows preserve
// protected public-layer metadata and are ignored by this required-action guard.

#include <catch2/catch_test_macros.hpp>
#include "entities/beat_map.h"
#include "components/obstacle.h"

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

    // Required (non-OnsetMarker) entries with their kind tag, merged across
    // shape_gate_beats and split_path_beats by beat_index (sorted independently
    // by parser). Per #1202/#1204, kind is identified by which vector the row
    // came from rather than a discriminator field.
    enum class RequiredKind { ShapeGate, SplitPath };
    struct RequiredEntry {
        RequiredKind kind;
        int beat_index;
    };

    for (const auto& path : beatmaps) {
        for (const char* difficulty : kDifficulties) {
            BeatMap map;
            std::vector<BeatMapError> errors;
            if (!load_beat_map(path, map, errors, difficulty)) continue;

            std::vector<RequiredEntry> required_beats;
            required_beats.reserve(map.shape_gate_beats.size() +
                                   map.split_path_beats.size());
            for (const auto& b : map.shape_gate_beats) {
                required_beats.push_back({RequiredKind::ShapeGate, b.beat_index});
            }
            for (const auto& b : map.split_path_beats) {
                required_beats.push_back({RequiredKind::SplitPath, b.beat_index});
            }
            std::stable_sort(required_beats.begin(), required_beats.end(),
                             [](const RequiredEntry& a, const RequiredEntry& b) {
                                 return a.beat_index < b.beat_index;
                             });
            if (required_beats.size() <= 1) continue;

            for (size_t index = 1; index < required_beats.size(); ++index) {
                const auto& left = required_beats[index - 1];
                const auto& right = required_beats[index];
                const int gap = right.beat_index - left.beat_index;

                if (gap != 1) continue;
                if (left.kind != RequiredKind::ShapeGate || right.kind != RequiredKind::ShapeGate) {
                    FAIL_CHECK(path << " [" << difficulty
                               << "] gap=1 contains non-shape-gate obstacle at beat "
                               << left.beat_index);
                }
            }
        }
    }
}
