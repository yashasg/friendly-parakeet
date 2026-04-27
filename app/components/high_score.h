#pragma once

#include <cstdint>
#include <map>
#include <string>

struct HighScoreState {
    std::map<std::string, int32_t> scores;
    std::string current_key;

    static std::string make_key(const std::string& song_id, const std::string& difficulty) {
        return song_id + "|" + difficulty;
    }

    int32_t get_current_high_score() const {
        if (current_key.empty()) return 0;
        auto it = scores.find(current_key);
        return it != scores.end() ? it->second : 0;
    }

    void update_current_high_score(int32_t score) {
        if (current_key.empty()) return;
        if (score < 0) score = 0;
        auto& stored = scores[current_key];
        if (score > stored) {
            stored = score;
        }
    }

    void set_current_key(const std::string& song_id, const std::string& difficulty) {
        current_key = make_key(song_id, difficulty);
    }
};

struct HighScorePersistence {
    std::string path;
};
