#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/input.h"
#include "../components/audio.h"
#include "../components/rhythm.h"
#include "../constants.h"

void player_action_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto& gesture = reg.ctx().get<GestureResult>();
    auto& btn_evt = reg.ctx().get<ShapeButtonEvent>();

    auto* song = reg.ctx().find<SongState>();
    bool rhythm_mode = song && song->playing;

    auto view = reg.view<PlayerTag, PlayerShape, Lane, VerticalState>();
    for (auto [entity, pshape, lane, vstate] : view.each()) {

        if (btn_evt.pressed && btn_evt.shape != Shape::Hexagon) {
            if (rhythm_mode) {
                auto phase = static_cast<WindowPhase>(pshape.phase_raw);

                if (phase == WindowPhase::Idle) {
                    // Start a new shape window
                    pshape.target_shape = btn_evt.shape;
                    pshape.previous = pshape.current;
                    pshape.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
                    pshape.window_timer = 0.0f;
                    pshape.window_start = song->song_time;
                    pshape.peak_time = song->song_time + song->morph_duration + song->half_window;
                    pshape.morph_t = 0.0f;
                    pshape.window_scale = 1.0f;
                    pshape.graded = false;
                    audio_push(reg.ctx().get<AudioQueue>(), SFX::ShapeShift);
                } else if (phase == WindowPhase::Active && btn_evt.shape != pshape.current) {
                    // Different shape mid-window: interrupt and restart
                    pshape.target_shape = btn_evt.shape;
                    pshape.previous = pshape.current;
                    pshape.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
                    pshape.window_timer = 0.0f;
                    pshape.window_start = song->song_time;
                    pshape.peak_time = song->song_time + song->morph_duration + song->half_window;
                    pshape.morph_t = 0.0f;
                    pshape.window_scale = 1.0f;
                    pshape.graded = false;
                    audio_push(reg.ctx().get<AudioQueue>(), SFX::ShapeShift);
                }
                // Same shape during Active → ignore (spam protection)
            } else {
                // Legacy mode: instant shape change
                if (btn_evt.shape != pshape.current) {
                    pshape.previous = pshape.current;
                    pshape.current  = btn_evt.shape;
                    pshape.morph_t  = 0.0f;
                    audio_push(reg.ctx().get<AudioQueue>(), SFX::ShapeShift);
                }
            }
        }

        // Lane change from swipe
        if (gesture.gesture == SwipeGesture::SwipeLeft && lane.current > 0) {
            lane.target = lane.current - 1;
            lane.lerp_t = 0.0f;
        }
        if (gesture.gesture == SwipeGesture::SwipeRight && lane.current < constants::LANE_COUNT - 1) {
            lane.target = lane.current + 1;
            lane.lerp_t = 0.0f;
        }

        // Jump / slide
        if (vstate.mode == VMode::Grounded) {
            if (gesture.gesture == SwipeGesture::SwipeUp) {
                vstate.mode  = VMode::Jumping;
                vstate.timer = constants::JUMP_DURATION;
            }
            if (gesture.gesture == SwipeGesture::SwipeDown) {
                vstate.mode  = VMode::Sliding;
                vstate.timer = constants::SLIDE_DURATION;
            }
        }
    }
}
