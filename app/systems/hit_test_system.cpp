#include "all_systems.h"
#include "../components/input_events.h"
#include "../components/input.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include <raylib.h>

void hit_test_system(entt::registry& reg) {
    auto& eq = reg.ctx().get<EventQueue>();
    auto  phase = reg.ctx().get<GameState>().phase;

    auto box_view = reg.view<Position, HitBox, ActiveInPhase>();
    auto circle_view = reg.view<Position, HitCircle, ActiveInPhase>();

    for (int i = 0; i < eq.input_count; ++i) {
        auto& evt = eq.inputs[i];

        if (evt.type == InputType::Swipe) {
            eq.push_go(evt.dir);
            continue;
        }

        Vector2 point = {evt.x, evt.y};

        // Tap: hit-test against HitBox entities
        for (auto [entity, pos, hb, aip] : box_view.each()) {
            if (!phase_active(aip, phase)) continue;
            Rectangle bounds = {
                pos.x - hb.half_w,
                pos.y - hb.half_h,
                hb.half_w * 2.0f,
                hb.half_h * 2.0f
            };
            if (CheckCollisionPointRec(point, bounds)) {
                eq.push_press(entity);
            }
        }

        // Tap: hit-test against HitCircle entities
        for (auto [entity, pos, hc, aip] : circle_view.each()) {
            if (!phase_active(aip, phase)) continue;
            if (CheckCollisionPointCircle(point, {pos.x, pos.y}, hc.radius)) {
                eq.push_press(entity);
            }
        }
    }
}
