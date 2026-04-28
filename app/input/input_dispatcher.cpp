#include "input_routing.h"
#include "../components/input_events.h"
#include "../ui/level_select_controller.h"
#include <entt/entt.hpp>
#include <stdexcept>

namespace {
struct InputDispatcherWiringState {
    bool wired = false;
};
}

// ── Drain-ownership model ────────────────────────────────────────────────────
// Two-tier delivery (#333):
//
// Tier 1 — Raw InputEvent (before fixed-step):
//   input_system enqueues InputEvent objects; game_loop_frame then calls
//   disp.update<InputEvent>(), which fires gesture_routing_handle_input and
//   hit_test_handle_input in registration order.  Those listeners enqueue
//   GoEvent / ButtonPressEvent as appropriate.
//
// Tier 2 — Semantic GoEvent / ButtonPressEvent (fixed-step delivery):
//   game_state_system (first in tick_fixed_systems) calls
//   disp.update<GoEvent>() + disp.update<ButtonPressEvent>(), draining the
//   semantic queues and delivering events to all registered listeners.
//   A second update<T>() later in the same tick (e.g., player_input_system)
//   finds an empty queue and is a guaranteed no-op.
//
// Listener registration order IS the canonical processing order (EnTT R1).
// Changing this order changes observable game behaviour.
void wire_input_dispatcher(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) {
        throw std::logic_error("wire_input_dispatcher requires entt::dispatcher in registry context");
    }

    auto* state = reg.ctx().find<InputDispatcherWiringState>();
    if (!state) {
        state = &reg.ctx().emplace<InputDispatcherWiringState>();
    }
    if (state->wired) {
        return;
    }

    // Tier-1: InputEvent → GoEvent / ButtonPressEvent (pre-fixed-step)
    disp->sink<InputEvent>().connect<&gesture_routing_handle_input>(reg);
    disp->sink<InputEvent>().connect<&hit_test_handle_input>(reg);

    // Tier-2: semantic events → handler callbacks (fixed-step)
    // Registration order = processing order: game_state first (handles phase
    // transitions, checks phase_timer), level_select second, player_input third.
    disp->sink<GoEvent>().connect<&game_state_handle_go>(reg);
    disp->sink<ButtonPressEvent>().connect<&game_state_handle_press>(reg);
    disp->sink<GoEvent>().connect<&level_select_handle_go>(reg);
    disp->sink<ButtonPressEvent>().connect<&level_select_handle_press>(reg);
    disp->sink<GoEvent>().connect<&player_input_handle_go>(reg);
    disp->sink<ButtonPressEvent>().connect<&player_input_handle_press>(reg);

    state->wired = true;
}

void unwire_input_dispatcher(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;
    disp->sink<InputEvent>().disconnect<&gesture_routing_handle_input>(reg);
    disp->sink<InputEvent>().disconnect<&hit_test_handle_input>(reg);
    disp->sink<GoEvent>().disconnect<&game_state_handle_go>(reg);
    disp->sink<ButtonPressEvent>().disconnect<&game_state_handle_press>(reg);
    disp->sink<GoEvent>().disconnect<&level_select_handle_go>(reg);
    disp->sink<ButtonPressEvent>().disconnect<&level_select_handle_press>(reg);
    disp->sink<GoEvent>().disconnect<&player_input_handle_go>(reg);
    disp->sink<ButtonPressEvent>().disconnect<&player_input_handle_press>(reg);
    if (auto* state = reg.ctx().find<InputDispatcherWiringState>()) {
        state->wired = false;
    }
}
