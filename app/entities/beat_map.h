#pragma once

#include "components/beat_map.h"
#include "components/song_state.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>

struct BeatMapTag {};

// Creates the singleton beat-map entity and returns its handle.
// Throws std::logic_error if the registry already has a BeatMapTag entity.
entt::entity create_beat_map_entity(entt::registry& reg, BeatMap map = {});

BeatMap* find_beat_map(entt::registry& reg);
const BeatMap* find_beat_map(const entt::registry& reg);
BeatMap& beat_map(entt::registry& reg);
const BeatMap& beat_map(const entt::registry& reg);

struct BeatMapError {
    int         beat_index = -1;
    std::string message;
};

// Validation limits used by validate_beat_map. Loaded from content/constants.json.
// Fields carry compiled-in defaults; any present key in the JSON file overrides them.
struct ValidationConstants {
    float bpm_min               = 60.0f;
    float bpm_max               = 300.0f;
    float offset_min            = -0.1f;
    float offset_max            = 5.0f;
    int   lead_beats_min        = 2;
    int   lead_beats_max        = 8;
    int   min_shape_change_gap  = 3;
};

// Load validation constants from content/constants.json.
// Tries the app-directory path first (explicit app_dir or GetApplicationDirectory()),
// then falls back to the CWD-relative "content/constants.json".
// Returns compiled-in defaults and logs a warning if neither file is found or parseable.
ValidationConstants load_validation_constants(const std::string& app_dir = "");

// Load a beat map from a JSON file. Returns true on success.
bool load_beat_map(const std::string& json_path, BeatMap& out,
                   std::vector<BeatMapError>& errors,
                   const std::string& difficulty = "medium");

// Load and validate a beat map from a JSON file. Returns true only when both
// parsing and validation succeed.
bool load_and_validate_beat_map(const std::string& json_path, BeatMap& out,
                                std::vector<BeatMapError>& errors,
                                const std::string& difficulty = "medium");

// Parse a beat map from a JSON string. Returns true on success.
//
bool parse_beat_map(const std::string& json_str, BeatMap& out,
                    std::vector<BeatMapError>& errors,
                    const std::string& difficulty = "medium");

// Validate a loaded beat map using explicitly provided constants.
bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors,
                       const ValidationConstants& vc);

// Validate a loaded beat map, loading constants from app-directory/CWD paths.
bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors);

// Initialize SongState from a loaded BeatMap.
void init_song_state(SongState& state, const BeatMap& map);
