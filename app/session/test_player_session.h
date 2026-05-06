#pragma once

#include <entt/entt.hpp>
#include <cstdint>

enum class TestPlayerSkill : uint8_t;

void test_player_init(entt::registry& reg, TestPlayerSkill skill,
                      const char* difficulty);

void test_player_shutdown(entt::registry& reg);
