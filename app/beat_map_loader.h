#pragma once

#include "components/rhythm.h"
#include <string>
#include <vector>

struct BeatMapError {
    int         beat_index = -1;
    std::string message;
};

// Load a beat map from a JSON file. Returns true on success.
bool load_beat_map(const std::string& json_path, BeatMap& out,
                   std::vector<BeatMapError>& errors,
                   const std::string& difficulty = "medium");

// Parse a beat map from a JSON string. Returns true on success.
bool parse_beat_map(const std::string& json_str, BeatMap& out,
                    std::vector<BeatMapError>& errors,
                    const std::string& difficulty = "medium");

// Validate a loaded beat map. Returns true if valid.
bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors);

// Initialize SongState from a loaded BeatMap.
void init_song_state(SongState& state, const BeatMap& map);
