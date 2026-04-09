#include "all_systems.h"
#include "../components/difficulty.h"
#include "../constants.h"
#include <algorithm>

void difficulty_system(entt::registry& reg, float dt) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed += dt;

    // Speed ramp: 1.0 → 3.0 over ~180s
    config.speed_multiplier = 1.0f + config.elapsed * constants::SPEED_RAMP_RATE;
    if (config.speed_multiplier > 3.0f) config.speed_multiplier = 3.0f;

    config.scroll_speed = constants::BASE_SCROLL_SPEED * config.speed_multiplier;

    // Spawn interval shrinks over time
    config.spawn_interval = constants::INITIAL_SPAWN_INT
        - config.elapsed * constants::SPAWN_RAMP_RATE;
    config.spawn_interval = std::max(config.spawn_interval, constants::MIN_SPAWN_INT);

    // Burnout window shrinks
    config.burnout_window_scale = 1.0f / (1.0f + config.elapsed * constants::BURNOUT_SHRINK);
}
