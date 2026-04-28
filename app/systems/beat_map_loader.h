#pragma once

#include "components/beat_map.h"
#include "components/song_state.h"
#include <string>
#include <vector>

struct BeatMapError {
    int         beat_index = -1;
    std::string message;
};

// Validation limits used by validate_beat_map. Loaded from content/constants.json.
// Fields carry compiled-in defaults; any present key in the JSON file overrides them.
struct ValidationConstants {
    float bpm_min               = 60.0f;
    float bpm_max               = 300.0f;
    float offset_min            = 0.0f;
    float offset_max            = 5.0f;
    int   lead_beats_min        = 2;
    int   lead_beats_max        = 8;
    int   min_shape_change_gap  = 3;
};

// Load validation constants from content/constants.json.
// If app_dir is non-empty, tries app_dir + "content/constants.json" first,
// then falls back to the CWD-relative "content/constants.json".
// Returns compiled-in defaults silently if neither file is found or parseable.
ValidationConstants load_validation_constants(const std::string& app_dir = "");

// Load a beat map from a JSON file. Returns true on success.
bool load_beat_map(const std::string& json_path, BeatMap& out,
                   std::vector<BeatMapError>& errors,
                   const std::string& difficulty = "medium");

// Parse a beat map from a JSON string. Returns true on success.
bool parse_beat_map(const std::string& json_str, BeatMap& out,
                    std::vector<BeatMapError>& errors,
                    const std::string& difficulty = "medium");

// Validate a loaded beat map using explicitly provided constants.
bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors,
                       const ValidationConstants& vc);

// Validate a loaded beat map, loading constants from CWD-relative path.
// Prefer the three-argument overload when the application directory is available
// (e.g. pass GetApplicationDirectory() to load_validation_constants first).
bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors);

// Initialize SongState from a loaded BeatMap.
void init_song_state(SongState& state, const BeatMap& map);
