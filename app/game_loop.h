#pragma once
#include <entt/entt.hpp>
#include <raylib.h>

// Run one frame: input → fixed timestep → render → blit → audio.
void game_loop_frame(entt::registry& reg, float& accumulator,
                     RenderTexture2D& target);
