#pragma once

#include <raylib.h>
#include <entt/entt.hpp>

enum class GameplayHudShapeSlot {
    Circle = 0,
    Square = 1,
    Triangle = 2
};


void init_gameplay_hud_screen_ui();
void render_gameplay_hud_screen_ui(entt::registry& reg);
Rectangle gameplay_hud_shape_input_bounds(GameplayHudShapeSlot slot);
void gameplay_hud_apply_button_presses(entt::registry& reg,
                                        bool pause_pressed,
                                        bool circle_pressed,
                                        bool square_pressed,
                                        bool triangle_pressed);
