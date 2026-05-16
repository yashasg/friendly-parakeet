#pragma once

#include <raylib.h>
#include <entt/entt.hpp>

#include "../../components/song_state.h"

// Approach-ring proximity readout (issue #1202 / #1204): the former
// `GameplayHudRingCue` discriminator switched on `Hidden/Far/Near/Perfect`,
// then re-switched to look up a draw color. Per Fabian's existential
// processing chapter the function now returns the data the renderer needs
// directly — visibility plus the ring color — instead of a label.
//
//   `visible == false`     → the ring is not drawn (former Hidden value).
//   `visible == true`      → ring is drawn with `color`. Far/Near/Perfect
//                            are encoded only by which `kGameplayHudRing*`
//                            constant `color` matches.
struct GameplayHudRingCue {
    bool  visible = false;
    Color color   = {0, 0, 0, 0};

    friend constexpr bool operator==(const GameplayHudRingCue& a,
                                     const GameplayHudRingCue& b) {
        return a.visible == b.visible
            && a.color.r == b.color.r && a.color.g == b.color.g
            && a.color.b == b.color.b && a.color.a == b.color.a;
    }
};

inline constexpr Color kGameplayHudRingPerfectColor{100, 255, 100, 255};
inline constexpr Color kGameplayHudRingNearColor   {180, 255, 100, 255};
inline constexpr Color kGameplayHudRingFarColor    {120, 120, 180, 255};

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
// Per-shape input bounds for the gameplay HUD. The former
// `gameplay_hud_shape_input_bounds(GameplayHudShapeSlot)` switched on a
// 3-value discriminator to pick one of these getters; per Fabian's
// existential processing (issues #1202 / #1204) each shape is its own
// named accessor — the type IS the choice, no runtime branch.
Rectangle gameplay_hud_circle_input_bounds();
Rectangle gameplay_hud_square_input_bounds();
Rectangle gameplay_hud_triangle_input_bounds();
void gameplay_hud_apply_button_presses(entt::registry& reg,
                                        bool pause_pressed,
                                        bool circle_pressed,
                                        bool square_pressed,
                                        bool triangle_pressed);
