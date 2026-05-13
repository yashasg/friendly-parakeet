#pragma once

#include <raylib.h>
#include <entt/entt.hpp>

#include "../../components/song_state.h"

enum class GameplayHudShapeSlot {
    Circle = 0,
    Square = 1,
    Triangle = 2
};

enum class GameplayHudRingCue {
    Hidden,
    Far,
    Near,
    Perfect
};

float gameplay_hud_perfect_distance(const SongState* song_state);
float gameplay_hud_good_distance(const SongState* song_state);
float gameplay_hud_ok_distance(const SongState* song_state);
float gameplay_hud_ring_ratio(float nearest_dist, float perfect_dist, float ring_appear_dist);
GameplayHudRingCue gameplay_hud_ring_cue(float nearest_dist,
                                         float perfect_dist,
                                         float good_dist,
                                         float ring_appear_dist);
void init_gameplay_hud_screen_ui();
void gameplay_hud_process_button_input(entt::registry& reg);
void render_gameplay_hud_screen_ui(entt::registry& reg);
Rectangle gameplay_hud_shape_input_bounds(GameplayHudShapeSlot slot);
void gameplay_hud_apply_button_presses(entt::registry& reg,
                                        bool pause_pressed,
                                        bool circle_pressed,
                                        bool square_pressed,
                                        bool triangle_pressed);
