#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::vector<fs::path> find_shipped_beatmaps_issue398() {
    std::vector<fs::path> paths;
    const fs::path dir{"content/beatmaps"};
    if (!fs::is_directory(dir)) return paths;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const auto name = entry.path().filename().string();
        if (name.ends_with("_beatmap.json")) {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

TEST_CASE("shipped beatmaps: onset metadata invariants", "[shipped_beatmaps][issue398][onset_metadata]") {
    constexpr std::array<const char*, 3> kDiffs = {"easy", "medium", "hard"};
    const std::set<std::string> kAllowedOnsetClasses = {
        "percussive",
        "harmonic",
        "full-spectrum",
    };

    REQUIRE(fs::is_directory("content/beatmaps"));
    const auto beatmaps = find_shipped_beatmaps_issue398();
    REQUIRE_FALSE(beatmaps.empty());

    for (const auto& path : beatmaps) {
        std::ifstream in(path);
        if (!in.is_open()) {
            FAIL_CHECK("onset metadata: failed to open " << path.string());
            continue;
        }

        json root;
        try {
            in >> root;
        } catch (const json::exception& e) {
            FAIL_CHECK("onset metadata: failed to parse " << path.string() << " (" << e.what() << ")");
            continue;
        }

        const auto difficulties_it = root.find("difficulties");
        if (difficulties_it == root.end() || !difficulties_it->is_object()) {
            FAIL_CHECK("onset metadata: missing difficulties object in " << path.string());
            continue;
        }

        std::array<int, 3> onset_counts{0, 0, 0};
        for (size_t diff_idx = 0; diff_idx < kDiffs.size(); ++diff_idx) {
            const std::string diff = kDiffs[diff_idx];
            const auto diff_it = difficulties_it->find(diff);
            if (diff_it == difficulties_it->end() || !diff_it->is_object()) {
                FAIL_CHECK("onset metadata: missing difficulty '" << diff << "' in " << path.string());
                continue;
            }

            const auto beats_it = diff_it->find("beats");
            if (beats_it == diff_it->end() || !beats_it->is_array()) {
                FAIL_CHECK("onset metadata: missing beats array for " << path.string() << " [" << diff << "]");
                continue;
            }

            if (beats_it->empty()) {
                FAIL_CHECK("onset metadata: empty beats for " << path.string() << " [" << diff << "]");
                continue;
            }

            for (size_t beat_idx = 0; beat_idx < beats_it->size(); ++beat_idx) {
                const auto& beat = (*beats_it)[beat_idx];
                if (!beat.is_object()) {
                    FAIL_CHECK("onset metadata: beat row is not an object for " << path.string()
                               << " [" << diff << "] idx=" << beat_idx);
                    continue;
                }

                const auto timing_it = beat.find("timing_source");
                if (timing_it == beat.end() || !timing_it->is_string()) {
                    FAIL_CHECK("onset metadata: missing timing_source for " << path.string()
                               << " [" << diff << "] idx=" << beat_idx);
                    continue;
                }

                const std::string timing_source = *timing_it;
                if (timing_source != "onset") {
                    FAIL_CHECK("onset metadata: timing_source must be 'onset' (no beat_fallback) for "
                               << path.string() << " [" << diff << "] idx=" << beat_idx
                               << " got='" << timing_source << "'");
                    continue;
                }
                ++onset_counts[diff_idx];

                const auto onset_time_it = beat.find("onset_time_sec");
                if (onset_time_it == beat.end() || !onset_time_it->is_number()) {
                    FAIL_CHECK("onset metadata: missing onset_time_sec for " << path.string()
                               << " [" << diff << "] idx=" << beat_idx);
                }

                const auto subdivision_it = beat.find("subdivision_label");
                if (subdivision_it == beat.end() || !subdivision_it->is_string() || subdivision_it->get<std::string>().empty()) {
                    FAIL_CHECK("onset metadata: missing subdivision_label for " << path.string()
                               << " [" << diff << "] idx=" << beat_idx);
                }

                const auto onset_class_it = beat.find("onset_class");
                if (onset_class_it == beat.end() || !onset_class_it->is_string()) {
                    FAIL_CHECK("onset metadata: missing onset_class for " << path.string()
                               << " [" << diff << "] idx=" << beat_idx);
                    continue;
                }
                const std::string onset_class = *onset_class_it;
                if (kAllowedOnsetClasses.find(onset_class) == kAllowedOnsetClasses.end()) {
                    FAIL_CHECK("onset metadata: invalid onset_class='" << onset_class << "' for "
                               << path.string() << " [" << diff << "] idx=" << beat_idx);
                }
            }
        }

        if (onset_counts[0] == 0 || onset_counts[1] == 0 || onset_counts[2] == 0) {
            FAIL_CHECK("onset metadata: each difficulty must retain onset-backed events for " << path.string()
                       << " (easy=" << onset_counts[0]
                       << ", medium=" << onset_counts[1]
                       << ", hard=" << onset_counts[2] << ")");
            continue;
        }

        if (onset_counts[0] > onset_counts[1] || onset_counts[1] > onset_counts[2]) {
            FAIL_CHECK("onset metadata: onset-backed counts must be non-decreasing (easy<=medium<=hard) for "
                       << path.string() << " got easy=" << onset_counts[0]
                       << " medium=" << onset_counts[1]
                       << " hard=" << onset_counts[2]);
        }
    }
}
