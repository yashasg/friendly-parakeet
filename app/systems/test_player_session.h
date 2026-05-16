#pragma once

#include <entt/entt.hpp>
#include <cstdint>
#include "test_player.h"
#include "../util/level_content_config.h"

struct TestPlayerSessionState {
    // Deterministic by default; callers may opt into runtime variability.
    uint32_t rng_seed = 0x5EED1234u;
    bool use_runtime_seed = false;

    // Monotonic sequence for deterministic log naming within a session.
    uint32_t log_sequence = 0;
};

void test_player_init(entt::registry& reg, SkillConfig skill,
                      const char* difficulty,
                      int selected_level = content_config::DEFAULT_LEVEL_INDEX);
