#pragma once

#include <entt/entt.hpp>

// Creates the canonical production player entity (Hexagon shape, center lane,
// Grounded vertical state) and returns its handle.  This is the single source
// of truth for player component layout and initial values; both the production
// play_session and rhythm-mode test helpers route through this factory.
entt::entity create_player_entity(entt::registry& reg);
