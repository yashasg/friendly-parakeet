#include "all_systems.h"
#include "../components/input_events.h"

// Routes Swipe inputs from the EventQueue into GoEvents. Tap and other input
// types are intentionally ignored here — spatial hit-testing is owned by
// hit_test_system (see #272).
//
// Iterates EventQueue::inputs in order so the relative ordering of GoEvents
// matches the order swipes arrived.
void gesture_routing_system(entt::registry& reg) {
    auto& eq   = reg.ctx().get<EventQueue>();
    auto& disp = reg.ctx().get<entt::dispatcher>();

    for (int i = 0; i < eq.input_count; ++i) {
        const auto& evt = eq.inputs[i];
        if (evt.type == InputType::Swipe) {
            disp.enqueue<GoEvent>({evt.dir});
        }
    }
}
