#include "input_routing.h"
#include "input_events.h"
#include "level_select_input_handlers.h"
#include <entt/entt.hpp>
#include <stdexcept>

namespace {
// Raw `entt::connection` (not `scoped_connection`) on purpose: see issue #1268.
// These live alongside the `entt::dispatcher` in `reg.ctx()`, which is a
// `dense_map<id_type, basic_any>` whose packed std::vector destruction order
// is unspecified across standard libraries (libc++ destroys in reverse,
// libstdc++ in forward order). A `scoped_connection` whose destructor fires
// after the dispatcher has already been destroyed would dereference a freed
// `sigh` and SIGSEGV in test teardown.
//
// With raw connections, registry teardown is a no-op for these handles: the
// listener vectors die with the dispatcher's `sigh`s, so there is nothing
// to disconnect. The `unwire_input_dispatcher()` path still has to release
// the listeners explicitly while the dispatcher is alive — see
// `release_all()` below — to honour "unwire preserves external listeners".
struct InputDispatcherConnections {
    entt::dispatcher* owner = nullptr;
    entt::connection go_up_game_state;
    entt::connection go_down_game_state;
    entt::connection go_left_game_state;
    entt::connection go_right_game_state;
    entt::connection menu_confirm_game_state;
    entt::connection menu_restart_game_state;
    entt::connection menu_go_level_select_game_state;
    entt::connection menu_go_main_menu_game_state;
    entt::connection go_up_level_select;
    entt::connection go_down_level_select;
    entt::connection go_left_level_select;
    entt::connection go_right_level_select;
    entt::connection menu_confirm_level_select;
    entt::connection menu_select_level_level_select;
    entt::connection menu_select_diff_level_select;
    entt::connection go_up_player_input;
    entt::connection go_down_player_input;
    entt::connection go_left_player_input;
    entt::connection go_right_player_input;
    entt::connection shape_press_circle_player_input;
    entt::connection shape_press_square_player_input;
    entt::connection shape_press_triangle_player_input;

    void release_all() noexcept {
        shape_press_triangle_player_input.release();
        shape_press_square_player_input.release();
        shape_press_circle_player_input.release();
        go_right_player_input.release();
        go_left_player_input.release();
        go_down_player_input.release();
        go_up_player_input.release();
        menu_select_diff_level_select.release();
        menu_select_level_level_select.release();
        menu_confirm_level_select.release();
        go_right_level_select.release();
        go_left_level_select.release();
        go_down_level_select.release();
        go_up_level_select.release();
        menu_go_main_menu_game_state.release();
        menu_go_level_select_game_state.release();
        menu_restart_game_state.release();
        menu_confirm_game_state.release();
        go_right_game_state.release();
        go_left_game_state.release();
        go_down_game_state.release();
        go_up_game_state.release();
        owner = nullptr;
    }
};

void warm_dispatcher_event_queues(entt::dispatcher& disp) {
    // Move first vector allocation out of the first gameplay input frame.
    disp.enqueue<GoUpEvent>(GoUpEvent{});
    disp.enqueue<GoDownEvent>(GoDownEvent{});
    disp.enqueue<GoLeftEvent>(GoLeftEvent{});
    disp.enqueue<GoRightEvent>(GoRightEvent{});
    disp.enqueue<ShapePressCircleEvent>(ShapePressCircleEvent{});
    disp.enqueue<ShapePressSquareEvent>(ShapePressSquareEvent{});
    disp.enqueue<ShapePressTriangleEvent>(ShapePressTriangleEvent{});
    disp.enqueue<MenuConfirmEvent>(MenuConfirmEvent{});
    disp.enqueue<MenuRestartEvent>(MenuRestartEvent{});
    disp.enqueue<MenuGoLevelSelectEvent>(MenuGoLevelSelectEvent{});
    disp.enqueue<MenuGoMainMenuEvent>(MenuGoMainMenuEvent{});
    disp.enqueue<MenuSelectLevelEvent>(MenuSelectLevelEvent{});
    disp.enqueue<MenuSelectDiffEvent>(MenuSelectDiffEvent{});
    disp.clear<GoUpEvent>();
    disp.clear<GoDownEvent>();
    disp.clear<GoLeftEvent>();
    disp.clear<GoRightEvent>();
    disp.clear<ShapePressCircleEvent>();
    disp.clear<ShapePressSquareEvent>();
    disp.clear<ShapePressTriangleEvent>();
    disp.clear<MenuConfirmEvent>();
    disp.clear<MenuRestartEvent>();
    disp.clear<MenuGoLevelSelectEvent>();
    disp.clear<MenuGoMainMenuEvent>();
    disp.clear<MenuSelectLevelEvent>();
    disp.clear<MenuSelectDiffEvent>();
}
}

// game_state_system owns the fixed-step drain of the per-direction Go*Event
// queues and the per-shape + per-action menu press event queues. EnTT sinks
// are last-connected first; connect in reverse semantic order
// (player_input → level_select → game_state) so game_state handlers fire
// first per the pre-#1277 contract.
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
    // Re-wire onto a different dispatcher: release any previously-wired
    // listeners on the previous dispatcher (assumed still alive — same
    // assumption the prior `scoped_connection` assignment relied on).
    state->release_all();
    state->owner = disp;

    // Fixed-step semantic order: game_state, level_select, player_input.
    // Shape presses route only to player_input (no menu listeners care about
    // them). Menu confirm routes to both game_state and level_select; the
    // remaining menu events route to a single consumer. Each cardinal
    // direction routes to all three consumers in the same reverse-connect
    // order so the game_state pause-resume guard fires before level_select
    // and player_input do their per-direction work.
    state->go_up_player_input =
        disp->sink<GoUpEvent>().connect<&player_input_handle_go_up>(reg);
    state->go_down_player_input =
        disp->sink<GoDownEvent>().connect<&player_input_handle_go_down>(reg);
    state->go_left_player_input =
        disp->sink<GoLeftEvent>().connect<&player_input_handle_go_left>(reg);
    state->go_right_player_input =
        disp->sink<GoRightEvent>().connect<&player_input_handle_go_right>(reg);
    state->shape_press_circle_player_input =
        disp->sink<ShapePressCircleEvent>().connect<&player_input_handle_press_circle>(reg);
    state->shape_press_square_player_input =
        disp->sink<ShapePressSquareEvent>().connect<&player_input_handle_press_square>(reg);
    state->shape_press_triangle_player_input =
        disp->sink<ShapePressTriangleEvent>().connect<&player_input_handle_press_triangle>(reg);
    state->go_up_level_select =
        disp->sink<GoUpEvent>().connect<&level_select_handle_go_up>(reg);
    state->go_down_level_select =
        disp->sink<GoDownEvent>().connect<&level_select_handle_go_down>(reg);
    state->go_left_level_select =
        disp->sink<GoLeftEvent>().connect<&level_select_handle_go_left>(reg);
    state->go_right_level_select =
        disp->sink<GoRightEvent>().connect<&level_select_handle_go_right>(reg);
    state->menu_confirm_level_select =
        disp->sink<MenuConfirmEvent>().connect<&level_select_handle_confirm>(reg);
    state->menu_select_level_level_select =
        disp->sink<MenuSelectLevelEvent>().connect<&level_select_handle_select_level>(reg);
    state->menu_select_diff_level_select =
        disp->sink<MenuSelectDiffEvent>().connect<&level_select_handle_select_diff>(reg);
    state->go_up_game_state =
        disp->sink<GoUpEvent>().connect<&game_state_handle_go_up>(reg);
    state->go_down_game_state =
        disp->sink<GoDownEvent>().connect<&game_state_handle_go_down>(reg);
    state->go_left_game_state =
        disp->sink<GoLeftEvent>().connect<&game_state_handle_go_left>(reg);
    state->go_right_game_state =
        disp->sink<GoRightEvent>().connect<&game_state_handle_go_right>(reg);
    state->menu_confirm_game_state =
        disp->sink<MenuConfirmEvent>().connect<&game_state_handle_confirm>(reg);
    state->menu_restart_game_state =
        disp->sink<MenuRestartEvent>().connect<&game_state_handle_restart>(reg);
    state->menu_go_level_select_game_state =
        disp->sink<MenuGoLevelSelectEvent>().connect<&game_state_handle_go_level_select>(reg);
    state->menu_go_main_menu_game_state =
        disp->sink<MenuGoMainMenuEvent>().connect<&game_state_handle_go_main_menu>(reg);

    warm_dispatcher_event_queues(*disp);
}

void unwire_input_dispatcher(entt::registry& reg) {
    if (auto* state = reg.ctx().find<InputDispatcherConnections>()) {
        state->release_all();
    }
}
