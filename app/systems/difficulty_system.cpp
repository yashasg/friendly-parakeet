#include "all_systems.h"
#include "../components/difficulty.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <algorithm>

void difficulty_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    // In rhythm mode, scroll speed is BPM-derived (SongState.scroll_speed).
    // Don't ramp speed/spawn/burnout — the beatmap controls difficulty.
    auto* song = reg.ctx().find<SongState>();
    if (song && song->playing) return;

    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed += dt;

    // Freeplay only: speed ramp 1.0 → 3.0 over ~180s
    config.speed_multiplier = 1.0f + config.elapsed * constants::SPEED_RAMP_RATE;
    if (config.speed_multiplier > 3.0f) config.speed_multiplier = 3.0f;

    config.scroll_speed = constants::BASE_SCROLL_SPEED * config.speed_multiplier;

    config.spawn_interval = constants::INITIAL_SPAWN_INT
        - config.elapsed * constants::SPAWN_RAMP_RATE;
    config.spawn_interval = std::max(config.spawn_interval, constants::MIN_SPAWN_INT);

    config.burnout_window_scale = 1.0f / (1.0f + config.elapsed * constants::BURNOUT_SHRINK);
}
