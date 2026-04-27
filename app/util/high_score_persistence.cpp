#include "high_score_persistence.h"
#include "settings_persistence.h"
#include "fs_utils.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <limits>

namespace high_score {

std::filesystem::path get_high_scores_dir() {
    return settings::get_settings_dir();
}

std::filesystem::path get_high_scores_file_path() {
    return get_high_scores_dir() / "high_scores.json";
}

nlohmann::json high_score_state_to_json(const HighScoreState& state) {
    nlohmann::json result;
    nlohmann::json scores_obj;
    
    for (const auto& [key, score] : state.scores) {
        scores_obj[key] = score;
    }
    
    result["scores"] = scores_obj;
    return result;
}

bool high_score_state_from_json(const nlohmann::json& obj, HighScoreState& state) {
    if (!obj.is_object()) {
        return false;
    }
    
    HighScoreState next;
    next.current_key = state.current_key;
    
    if (obj.contains("scores")) {
        const auto& scores_obj = obj["scores"];
        if (!scores_obj.is_object()) {
            return false;
        }
        
        for (auto it = scores_obj.begin(); it != scores_obj.end(); ++it) {
            const auto& key = it.key();
            const auto& value = it.value();
            
            // Validate score is a valid integer
            if (!value.is_number_integer()) {
                return false;
            }
            
            // Get score and clamp to valid range
            std::int64_t raw = 0;
            if (value.is_number_unsigned()) {
                const auto unsigned_raw = value.get<std::uint64_t>();
                raw = unsigned_raw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
                    ? std::numeric_limits<std::int64_t>::max()
                    : static_cast<std::int64_t>(unsigned_raw);
            } else {
                raw = value.get<std::int64_t>();
            }
            
            // Clamp negative scores to 0, cap at max int32
            if (raw < 0) {
                raw = 0;
            } else if (raw > std::numeric_limits<std::int32_t>::max()) {
                raw = std::numeric_limits<std::int32_t>::max();
            }
            
            next.scores[key] = static_cast<std::int32_t>(raw);
        }
    }
    
    state = next;
    return true;
}

bool load_high_scores(HighScoreState& state, const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return false;
    }
    
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        nlohmann::json obj;
        file >> obj;
        return high_score_state_from_json(obj, state);
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

bool save_high_scores(const HighScoreState& state, const std::filesystem::path& path) {
    if (!fs_utils::ensure_directory(path.parent_path())) {
        return false;
    }
    
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    nlohmann::json obj = high_score_state_to_json(state);
    file << obj.dump(2);
    file.flush();
    if (!file.good()) {
        return false;
    }
    file.close();
    return true;
}

}  // namespace high_score
