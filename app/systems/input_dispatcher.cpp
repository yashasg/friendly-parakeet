#include "all_systems.h"
#include "../components/input_events.h"
#include <entt/entt.hpp>

void wire_input_dispatcher(entt::registry& reg) {
    auto& disp = reg.ctx().get<entt::dispatcher>();
    // Registration order = processing order: game_state first (handles phase transitions,
    // checks phase_timer), level_select second, player_input third.
    disp.sink<GoEvent>().connect<&game_state_handle_go>(reg);
    disp.sink<ButtonPressEvent>().connect<&game_state_handle_press>(reg);
    disp.sink<GoEvent>().connect<&level_select_handle_go>(reg);
    disp.sink<ButtonPressEvent>().connect<&level_select_handle_press>(reg);
    disp.sink<GoEvent>().connect<&player_input_handle_go>(reg);
    disp.sink<ButtonPressEvent>().connect<&player_input_handle_press>(reg);
}

void unwire_input_dispatcher(entt::registry& reg) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (!disp) return;
    disp->sink<GoEvent>().disconnect();
    disp->sink<ButtonPressEvent>().disconnect();
}
