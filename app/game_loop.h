#pragma once
#include <entt/entt.hpp>
#include <raylib.h>

// Run one frame: input → fixed timestep → render → blit → audio.
// Called each iteration of the native loop or by emscripten_set_main_loop.
void game_loop_frame(entt::registry& reg, float& accumulator,
                     RenderTexture2D& target);

// Emscripten entry point — wraps game_loop_frame for the browser event loop.
#ifdef __EMSCRIPTEN__
void game_loop_init_emscripten(entt::registry& reg, RenderTexture2D& target);
#endif
