#pragma once

#include <entt/entt.hpp>
#include <raylib.h>

#include "../../components/game_state.h"

void init_game_over_screen_ui();
void render_game_over_screen_ui(entt::registry& reg);

using GameOverLayoutRenderHook = void (*)(struct GameOverLayoutState* state);
using GameOverValueDrawHook = void (*)(Vector2 anchor, float x, float y, float w, float h,
                                       const char* text, int text_size);

void set_game_over_screen_test_hooks(GameOverLayoutRenderHook layout_render_hook,
                                     GameOverValueDrawHook value_draw_hook);
void reset_game_over_screen_test_hooks();
