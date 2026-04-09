#include "all_systems.h"
#include "../components/player.h"
#include "../components/input.h"
#include "../components/audio.h"
#include "../constants.h"

void player_action_system(entt::registry& reg, float /*dt*/) {
    auto& gesture = reg.ctx().get<GestureResult>();
    auto& btn_evt = reg.ctx().get<ShapeButtonEvent>();

    auto view = reg.view<PlayerTag, PlayerShape, Lane, VerticalState>();
    for (auto [entity, pshape, lane, vstate] : view.each()) {

        // Shape change from button tap
        if (btn_evt.pressed && btn_evt.shape != pshape.current) {
            pshape.previous = pshape.current;
            pshape.current  = btn_evt.shape;
            pshape.morph_t  = 0.0f;
            reg.ctx().get<AudioQueue>().push(SFX::ShapeShift);
        }

        // Lane change from swipe
        if (gesture.gesture == Gesture::SwipeLeft && lane.current > 0) {
            lane.target = lane.current - 1;
            lane.lerp_t = 0.0f;
        }
        if (gesture.gesture == Gesture::SwipeRight && lane.current < constants::LANE_COUNT - 1) {
            lane.target = lane.current + 1;
            lane.lerp_t = 0.0f;
        }

        // Jump / slide
        if (vstate.mode == VMode::Grounded) {
            if (gesture.gesture == Gesture::SwipeUp) {
                vstate.mode  = VMode::Jumping;
                vstate.timer = constants::JUMP_DURATION;
            }
            if (gesture.gesture == Gesture::SwipeDown) {
                vstate.mode  = VMode::Sliding;
                vstate.timer = constants::SLIDE_DURATION;
            }
        }
    }
}
