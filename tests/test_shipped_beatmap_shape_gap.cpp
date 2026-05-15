// Regression coverage for shipped beatmaps.
//
// CWD when running via CTest is the build directory, which has a copy of
// content/ created by CMake POST_BUILD commands.  The same relative path
// ("content/beatmaps/...") therefore works both when running the binary
// directly from build/ and through `ctest`.
//

#include <catch2/catch_test_macros.hpp>
#include "entities/beat_map.h"

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
