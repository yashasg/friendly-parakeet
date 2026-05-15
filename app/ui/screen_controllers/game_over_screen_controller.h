#pragma once

#include <entt/entt.hpp>
#include <raylib.h>

#include "../../components/game_state.h"

void init_game_over_screen_ui();
void render_game_over_screen_ui(entt::registry& reg);

// Maps a DeathCause to a short, colorblind-safe, platform-neutral one-line
// reason string suitable for the Game Over ReasonSlot. Returns "" for None.
const char* death_cause_text(DeathCause cause);

using GameOverLayoutRenderHook = void (*)(struct GameOverLayoutState* state);
using GameOverValueDrawHook = void (*)(Vector2 anchor, float x, float y, float w, float h,
                                       const char* text, int text_size);

void set_game_over_screen_test_hooks(GameOverLayoutRenderHook layout_render_hook,
                                     GameOverValueDrawHook value_draw_hook);
void reset_game_over_screen_test_hooks();
