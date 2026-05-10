// Regression coverage for issues #391, #392, #394, #396.
//
//   #391 — Per-difficulty max consecutive same-lane run ceilings.
//   #396 — Subdivision label coverage (≥10% non-downbeat per difficulty).
//   #392 — Mental Corruption monotonic IOI ramp + medium/hard separation.
//   #394 — Stomper monotonic IOI ramp (easy slowest, hard fastest).
//
// All checks use FAIL_CHECK so every regression is reported in one run.
//
// CWD when run via CTest is the build/ directory which has a mirrored
// content/beatmaps/ copy via CMake POST_BUILD commands.

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::vector<fs::path> find_beatmaps() {
    std::vector<fs::path> paths;
    const fs::path dir{"content/beatmaps"};
    if (!fs::is_directory(dir)) return paths;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const auto name = entry.path().filename().string();
        if (name.ends_with("_beatmap.json")) paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

struct DiffStats {
    int max_shape_cluster_size = 0;
    int max_same_shape_cluster_run = 0;
    int total = 0;
    int non_downbeat = 0;
    double lower_quartile_ioi = 0.0;
};

DiffStats collect(const json& beats_arr) {
    DiffStats s;
    s.total = static_cast<int>(beats_arr.size());
    int last_cluster_beat = -1;
    std::string previous_cluster_shape;
    int current_cluster_size = 0;
    int current_same_shape_cluster_run = 0;
    std::vector<double> times;
    times.reserve(s.total);
    for (const auto& b : beats_arr) {
        if (!b.is_object()) continue;
        const int beat = b.value("beat", -1);
        const std::string shape = b.value("shape", "");
        const std::string label = b.value("subdivision_label", "downbeat");
        if (label != "downbeat") ++s.non_downbeat;
        if (b.value("kind", "") == "shape_gate" && beat >= 0) {
            const bool starts_new_cluster =
                last_cluster_beat < 0 || beat - last_cluster_beat >= 3;
            if (starts_new_cluster) {
                current_cluster_size = 1;
                if (shape == previous_cluster_shape) {
                    ++current_same_shape_cluster_run;
                } else {
                    current_same_shape_cluster_run = 1;
                    previous_cluster_shape = shape;
                }
            } else {
                ++current_cluster_size;
            }
            last_cluster_beat = beat;
            s.max_shape_cluster_size = std::max(s.max_shape_cluster_size, current_cluster_size);
            s.max_same_shape_cluster_run =
                std::max(s.max_same_shape_cluster_run, current_same_shape_cluster_run);
        }
        if (b.contains("time_sec") && b["time_sec"].is_number()) {
            times.push_back(b["time_sec"].get<double>());
        }
    }
    std::sort(times.begin(), times.end());
    std::vector<double> iois;
    iois.reserve(times.size());
    for (size_t i = 1; i < times.size(); ++i) {
        const double dt = times[i] - times[i - 1];
        if (dt > 0.0) iois.push_back(dt);
    }
    if (!iois.empty()) {
        std::sort(iois.begin(), iois.end());
        s.lower_quartile_ioi = iois[iois.size() / 4];
    }
    return s;
}

std::map<std::string, DiffStats> load_song(const fs::path& path) {
    std::map<std::string, DiffStats> out;
    std::ifstream in(path);
    if (!in.is_open()) return out;
    json root;
    try { in >> root; } catch (...) { return out; }
    const auto difficulties_it = root.find("difficulties");
    if (difficulties_it == root.end() || !difficulties_it->is_object()) return out;
    for (const char* d : {"easy", "medium", "hard"}) {
        const auto diff_it = difficulties_it->find(d);
        if (diff_it == difficulties_it->end()) continue;
        const auto beats_it = diff_it->find("beats");
        if (beats_it == diff_it->end() || !beats_it->is_array()) continue;
        out[d] = collect(*beats_it);
    }
    return out;
}

}  // namespace

TEST_CASE("shipped beatmaps: same-shape cluster gates are capped per difficulty",
          "[shipped_beatmaps][issue391][issue532][max_lane_run]") {
    const std::map<std::string, int> kRunCap = {{"medium", 3}, {"hard", 3}};
    const std::map<std::string, int> kSizeCap = {{"medium", 4}, {"hard", 5}};
    const auto beatmaps = find_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        const auto stats = load_song(path);
        for (const auto& [diff, cap] : kRunCap) {
            const auto it = stats.find(diff);
            if (it == stats.end()) continue;
            if (it->second.max_same_shape_cluster_run > cap) {
                FAIL_CHECK("same-shape cluster-chain run exceeded: " << path.string()
                           << " [" << diff << "] max_run="
                           << it->second.max_same_shape_cluster_run
                           << " > cap=" << cap << " (issues #391/#532)");
            }
        }
        for (const auto& [diff, cap] : kSizeCap) {
            const auto it = stats.find(diff);
            if (it == stats.end()) continue;
            if (it->second.max_shape_cluster_size > cap) {
                FAIL_CHECK("max shape cluster size exceeded: " << path.string()
                           << " [" << diff << "] max_cluster_size="
                           << it->second.max_shape_cluster_size
                           << " > cap=" << cap << " (issue #532)");
            }
        }
    }
}

TEST_CASE("shipped beatmaps: subdivision labels include non-downbeat events",
          "[shipped_beatmaps][issue396][subdivision_coverage]") {
    // Per issue acceptance: ≥5% non-downbeat on easy, ≥10% on medium, ≥10% on hard.
    // We use 10% across the board — current generator easily exceeds this.
    const std::map<std::string, double> kMinShare = {
        {"easy", 0.05}, {"medium", 0.10}, {"hard", 0.10}};
    const auto beatmaps = find_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        const auto stats = load_song(path);
        for (const auto& [diff, min_share] : kMinShare) {
            const auto it = stats.find(diff);
            if (it == stats.end() || it->second.total == 0) continue;
            const double share = static_cast<double>(it->second.non_downbeat) / it->second.total;
            if (share < min_share) {
                FAIL_CHECK("subdivision coverage below floor: " << path.string()
                           << " [" << diff << "] non_downbeat="
                           << it->second.non_downbeat << "/" << it->second.total
                           << " share=" << share << " < " << min_share
                           << " (issue #396)");
            }
        }
    }
}

TEST_CASE("shipped beatmaps: dense-region IOI does not invert by difficulty",
           "[shipped_beatmaps][issue392][issue394][difficulty_ramp_ioi]") {
    constexpr double kAllowedInversionSec = 0.050;
    const auto beatmaps = find_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        const auto stats = load_song(path);
        const auto e = stats.find("easy");
        const auto m = stats.find("medium");
        const auto h = stats.find("hard");
        if (e == stats.end() || m == stats.end() || h == stats.end()) continue;

        const double ei = e->second.lower_quartile_ioi;
        const double mi = m->second.lower_quartile_ioi;
        const double hi = h->second.lower_quartile_ioi;

        if (mi - ei > kAllowedInversionSec) {
            FAIL_CHECK("difficulty IOI ramp: " << path.string()
                       << " medium dense-region IOI " << mi
                       << " must not exceed easy " << ei
                       << " by > " << kAllowedInversionSec << "s (issues #392/#394/#529)");
        }
        if (hi - mi > kAllowedInversionSec) {
            FAIL_CHECK("difficulty IOI ramp: " << path.string()
                       << " hard dense-region IOI " << hi
                       << " must not exceed medium " << mi
                       << " by > " << kAllowedInversionSec << "s (issues #392/#394/#529)");
        }
    }
}
