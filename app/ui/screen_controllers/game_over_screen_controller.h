#pragma once

#include <entt/entt.hpp>

#include "../../components/song_state.h"

void init_game_over_screen_ui();
void render_game_over_screen_ui(entt::registry& reg);

// Maps a DeathCause to a short, colorblind-safe, platform-neutral one-line
// reason string suitable for the Game Over ReasonSlot. Returns "" for None.
const char* death_cause_text(DeathCause cause);
