#pragma once

#include <cstdint>
#include <entt/entt.hpp>

enum class TestPlayerSkill : uint8_t;

[[nodiscard]] bool game_loop_runtime_init(entt::registry& reg,
                                          bool test_player_mode,
                                          TestPlayerSkill test_skill,
                                          const char* difficulty);

void game_loop_runtime_frame(entt::registry& reg,
                             float& accumulator,
                             float fixed_dt,
                             float max_accumulator);

[[nodiscard]] bool game_loop_runtime_should_quit(const entt::registry& reg);

void game_loop_runtime_shutdown(entt::registry& reg);
