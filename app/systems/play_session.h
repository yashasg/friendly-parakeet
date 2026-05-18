#pragma once

#include <entt/entt.hpp>
#include <string>

struct PlaySessionContentOverride {
    std::string beatmap_path;
    std::string difficulty_key;
};

// Sets up a fresh play session: clears entities, resets singletons,
// creates the player entity. Called by game_state_system on the
// transition into GamePhasePlayingTag (see app/tags/tags.h).
void setup_play_session(entt::registry& reg);
