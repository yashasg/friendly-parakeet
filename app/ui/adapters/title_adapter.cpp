// Title screen adapter - bridges generated rguilayout output to game systems

#include "../../components/game_state.h"
#include "../../components/ui_state.h"
#include "../../components/input.h"
#include <entt/entt.hpp>
#include <raylib.h>

#include "../vendor/raygui.h"
#include "../generated/title_layout.h"

namespace {

TitleLayoutState layout_state;
bool initialized = false;

} // anonymous namespace

void title_adapter_init() {
    if (!initialized) {
        layout_state = TitleLayout_Init();
        initialized = true;
    }
}

void title_adapter_render(entt::registry& reg) {
    if (!initialized) {
        title_adapter_init();
    }

    TitleLayout_Render(&layout_state);

#ifndef PLATFORM_WEB
    if (layout_state.ExitButtonPressed) {
        reg.ctx().get<InputState>().quit_requested = true;
    }
#endif

    if (layout_state.SettingsButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Settings;
    }
}
