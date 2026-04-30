#include "input_routing.h"
#include "../components/input_events.h"

// Routes Swipe InputEvents into GoEvents.  Tap and other input types are
// intentionally ignored here.
//
// Registered as a disp.sink<InputEvent>() listener in wire_input_dispatcher()
// and fired via disp.update<InputEvent>() in game_loop_frame before the
// fixed-step accumulator loop (#333).
void gesture_routing_handle_input(entt::registry& reg, const InputEvent& evt) {
    if (evt.type != InputType::Swipe) return;
    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({evt.dir});
}
