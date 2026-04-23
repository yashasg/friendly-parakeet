#include "all_systems.h"
#include "../components/input_events.h"
#include "../components/input.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include <cmath>

void hit_test_system(entt::registry& reg) {
    auto& eq = reg.ctx().get<EventQueue>();
    auto  phase = reg.ctx().get<GameState>().phase;

    for (int i = 0; i < eq.input_count; ++i) {
        auto& evt = eq.inputs[i];

        if (evt.type == InputType::Swipe) {
            eq.push_go(evt.dir);
            continue;
        }

        // Tap: hit-test against HitBox entities
        {
            auto view = reg.view<Position, HitBox, ActiveInPhase>();
            for (auto [entity, pos, hb, aip] : view.each()) {
                if (!phase_active(aip, phase)) continue;
                if (std::abs(evt.x - pos.x) <= hb.half_w &&
                    std::abs(evt.y - pos.y) <= hb.half_h) {
                    eq.push_press(entity);
                }
            }
        }

        // Tap: hit-test against HitCircle entities
        {
            auto view = reg.view<Position, HitCircle, ActiveInPhase>();
            for (auto [entity, pos, hc, aip] : view.each()) {
                if (!phase_active(aip, phase)) continue;
                float dx = evt.x - pos.x;
                float dy = evt.y - pos.y;
                if (dx * dx + dy * dy <= hc.radius * hc.radius) {
                    eq.push_press(entity);
                }
            }
        }
    }
}
