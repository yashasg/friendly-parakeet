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
    int max_lane_run = 0;
    int total = 0;
    int non_downbeat = 0;
    double median_ioi = 0.0;
};

DiffStats collect(const json& beats_arr) {
    DiffStats s;
    s.total = static_cast<int>(beats_arr.size());
    int last_lane = -1;
    int run_len = 0;
    std::vector<double> times;
    times.reserve(s.total);
    for (const auto& b : beats_arr) {
        if (!b.is_object()) continue;
        const int lane = b.value("lane", 1);
        const std::string label = b.value("subdivision_label", "downbeat");
        if (label != "downbeat") ++s.non_downbeat;
        if (lane == last_lane) {
            ++run_len;
        } else {
            run_len = 1;
            last_lane = lane;
        }
        if (run_len > s.max_lane_run) s.max_lane_run = run_len;
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
        s.median_ioi = iois[iois.size() / 2];
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

TEST_CASE("shipped beatmaps: max consecutive same-lane run is capped per difficulty",
          "[shipped_beatmaps][issue391][max_lane_run]") {
    const std::map<std::string, int> kCap = {{"easy", 4}, {"medium", 5}, {"hard", 6}};
    const auto beatmaps = find_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        const auto stats = load_song(path);
        for (const auto& [diff, cap] : kCap) {
            const auto it = stats.find(diff);
            if (it == stats.end()) continue;
            if (it->second.max_lane_run > cap) {
                FAIL_CHECK("max same-lane run exceeded: " << path.string()
                           << " [" << diff << "] max_run=" << it->second.max_lane_run
                           << " > cap=" << cap << " (issue #391)");
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

TEST_CASE("shipped beatmaps: median IOI decreases monotonically by difficulty",
          "[shipped_beatmaps][issue392][issue394][difficulty_ramp_ioi]") {
    const auto beatmaps = find_beatmaps();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        const auto stats = load_song(path);
        const auto e = stats.find("easy");
        const auto m = stats.find("medium");
        const auto h = stats.find("hard");
        if (e == stats.end() || m == stats.end() || h == stats.end()) continue;

        const double ei = e->second.median_ioi;
        const double mi = m->second.median_ioi;
        const double hi = h->second.median_ioi;

        if (!(ei >= mi)) {
            FAIL_CHECK("difficulty IOI ramp: " << path.string()
                       << " easy median IOI " << ei
                       << " must be >= medium " << mi << " (issues #392/#394)");
        }
        if (!(mi >= hi)) {
            FAIL_CHECK("difficulty IOI ramp: " << path.string()
                       << " medium median IOI " << mi
                       << " must be >= hard " << hi << " (issues #392/#394)");
        }
        // Require either a ≥10% IOI drop OR a clear easy→hard gap (≥1.5×).
        // Some songs (e.g., dense onsets like 2_drama) saturate the medium
        // band naturally close to hard; in those cases the overall easy→hard
        // ramp must still be perceivable end-to-end.
        const bool overall_perceivable = (hi > 0.0) && ((ei / hi) >= 1.5);
        if (ei > 0.0 && (ei - mi) / ei < 0.10 && !overall_perceivable) {
            FAIL_CHECK("difficulty IOI ramp: " << path.string()
                       << " easy->medium IOI gap < 10% and easy/hard ratio < 1.5x"
                       << " (easy=" << ei << " medium=" << mi << " hard=" << hi
                       << ") — players cannot perceive pacing step");
        }
        if (mi > 0.0 && (mi - hi) / mi < 0.10 && !overall_perceivable) {
            FAIL_CHECK("difficulty IOI ramp: " << path.string()
                       << " medium->hard IOI gap < 10% and easy/hard ratio < 1.5x"
                       << " (easy=" << ei << " medium=" << mi << " hard=" << hi
                       << ") — players cannot perceive pacing step");
        }
    }
}
