#include "input_routing.h"
#include "input_events.h"
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

// game_state_system owns the fixed-step GoEvent/ButtonPressEvent drain.
// EnTT sinks are last-connected first; connect in reverse semantic order.
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

    // Fixed-step semantic order: game_state, level_select, player_input.
    state->go_player_input = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&player_input_handle_go>(reg)};
    state->press_player_input = entt::scoped_connection{
        disp->sink<ButtonPressEvent>().connect<&player_input_handle_press>(reg)};
    state->go_level_select = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&level_select_handle_go>(reg)};
    state->press_level_select = entt::scoped_connection{
        disp->sink<ButtonPressEvent>().connect<&level_select_handle_press>(reg)};
    state->go_game_state = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&game_state_handle_go>(reg)};
    state->press_game_state = entt::scoped_connection{
        disp->sink<ButtonPressEvent>().connect<&game_state_handle_press>(reg)};

    warm_dispatcher_event_queues(*disp);
}

void unwire_input_dispatcher(entt::registry& reg) {
    if (auto* state = reg.ctx().find<InputDispatcherConnections>()) {
        *state = InputDispatcherConnections{};
    }
}
