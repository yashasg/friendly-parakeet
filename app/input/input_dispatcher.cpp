#include "input_routing.h"
#include "../components/input_events.h"
#include "../ui/level_select_controller.h"
#include <entt/entt.hpp>
#include <stdexcept>

namespace {
struct InputDispatcherConnections {
    entt::dispatcher* owner = nullptr;
    entt::scoped_connection go_game_state;
    entt::scoped_connection press_game_state;
    entt::scoped_connection go_level_select;
    entt::scoped_connection press_level_select;
    entt::scoped_connection go_player_input;
    entt::scoped_connection press_player_input;
};

void warm_dispatcher_event_queues(entt::dispatcher& disp) {
    // Move first vector allocation out of the first gameplay input frame.
    disp.enqueue<GoEvent>(GoEvent{});
    disp.enqueue<ButtonPressEvent>(ButtonPressEvent{});
    disp.clear<GoEvent>();
    disp.clear<ButtonPressEvent>();
}
}

// ── Drain-ownership model ────────────────────────────────────────────────────
// game_state_system (first in tick_fixed_systems) owns the authoritative
// fixed-step drain for GoEvent and ButtonPressEvent.
//
// Listener registration order IS the canonical processing order (EnTT R1).
// Changing this order changes observable game behaviour.
void wire_input_dispatcher(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) {
        throw std::logic_error("wire_input_dispatcher requires entt::dispatcher in registry context");
    }

    auto* state = reg.ctx().find<InputDispatcherConnections>();
    if (!state) {
        state = &reg.ctx().emplace<InputDispatcherConnections>();
    }
    if (state->owner == disp) {
        return;
    }
    *state = InputDispatcherConnections{};
    state->owner = disp;

    // Semantic events → handler callbacks (fixed-step)
    // Registration order = processing order: game_state first (handles phase
    // transitions, checks phase_timer), level_select second, player_input third.
    state->go_game_state = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&game_state_handle_go>(reg)};
    state->press_game_state = entt::scoped_connection{
        disp->sink<ButtonPressEvent>().connect<&game_state_handle_press>(reg)};
    state->go_level_select = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&level_select_handle_go>(reg)};
    state->press_level_select = entt::scoped_connection{
        disp->sink<ButtonPressEvent>().connect<&level_select_handle_press>(reg)};
    state->go_player_input = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&player_input_handle_go>(reg)};
    state->press_player_input = entt::scoped_connection{
        disp->sink<ButtonPressEvent>().connect<&player_input_handle_press>(reg)};

    warm_dispatcher_event_queues(*disp);
}

void unwire_input_dispatcher(entt::registry& reg) {
    if (auto* state = reg.ctx().find<InputDispatcherConnections>()) {
        *state = InputDispatcherConnections{};
    }
}
