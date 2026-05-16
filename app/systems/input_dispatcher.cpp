#include "input_routing.h"
#include "input_events.h"
#include "../ui/level_select_controller.h"
#include <entt/entt.hpp>
#include <stdexcept>

namespace {
struct InputDispatcherConnections {
    entt::dispatcher* owner = nullptr;
    entt::scoped_connection go_game_state;
    entt::scoped_connection menu_press_game_state;
    entt::scoped_connection go_level_select;
    entt::scoped_connection menu_press_level_select;
    entt::scoped_connection go_player_input;
    entt::scoped_connection shape_press_circle_player_input;
    entt::scoped_connection shape_press_square_player_input;
    entt::scoped_connection shape_press_triangle_player_input;
};

void warm_dispatcher_event_queues(entt::dispatcher& disp) {
    // Move first vector allocation out of the first gameplay input frame.
    disp.enqueue<GoEvent>(GoEvent{});
    disp.enqueue<ShapePressCircleEvent>(ShapePressCircleEvent{});
    disp.enqueue<ShapePressSquareEvent>(ShapePressSquareEvent{});
    disp.enqueue<ShapePressTriangleEvent>(ShapePressTriangleEvent{});
    disp.enqueue<MenuPressEvent>(MenuPressEvent{});
    disp.clear<GoEvent>();
    disp.clear<ShapePressCircleEvent>();
    disp.clear<ShapePressSquareEvent>();
    disp.clear<ShapePressTriangleEvent>();
    disp.clear<MenuPressEvent>();
}
}

// game_state_system owns the fixed-step drain of GoEvent and the per-shape
// + menu press event queues. EnTT sinks are last-connected first; connect in
// reverse semantic order (player_input → level_select → game_state).
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
    // Shape presses route only to player_input (no menu listeners care about
    // them). Menu presses route to both game_state and level_select.
    state->go_player_input = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&player_input_handle_go>(reg)};
    state->shape_press_circle_player_input = entt::scoped_connection{
        disp->sink<ShapePressCircleEvent>().connect<&player_input_handle_press_circle>(reg)};
    state->shape_press_square_player_input = entt::scoped_connection{
        disp->sink<ShapePressSquareEvent>().connect<&player_input_handle_press_square>(reg)};
    state->shape_press_triangle_player_input = entt::scoped_connection{
        disp->sink<ShapePressTriangleEvent>().connect<&player_input_handle_press_triangle>(reg)};
    state->go_level_select = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&level_select_handle_go>(reg)};
    state->menu_press_level_select = entt::scoped_connection{
        disp->sink<MenuPressEvent>().connect<&level_select_handle_press_menu>(reg)};
    state->go_game_state = entt::scoped_connection{
        disp->sink<GoEvent>().connect<&game_state_handle_go>(reg)};
    state->menu_press_game_state = entt::scoped_connection{
        disp->sink<MenuPressEvent>().connect<&game_state_handle_press_menu>(reg)};

    warm_dispatcher_event_queues(*disp);
}

void unwire_input_dispatcher(entt::registry& reg) {
    if (auto* state = reg.ctx().find<InputDispatcherConnections>()) {
        *state = InputDispatcherConnections{};
    }
}
