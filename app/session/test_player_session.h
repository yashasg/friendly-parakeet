#pragma once

#include <entt/entt.hpp>
#include "../components/test_player.h"

void test_player_init(entt::registry& reg, TestPlayerSkill skill,
                      const char* difficulty);
