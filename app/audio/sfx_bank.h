#pragma once

#include <entt/entt.hpp>

void sfx_bank_init(entt::registry& reg);
void sfx_playback_backend_init(entt::registry& reg);
void sfx_bank_unload(entt::registry& reg);
