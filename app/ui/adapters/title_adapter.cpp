// Title screen adapter - bridges generated rguilayout output to game systems

#include "../../components/game_state.h"
#include "../../components/ui_state.h"
#include "../../components/input.h"
#include "adapter_base.h"
#include <entt/entt.hpp>

#include "../vendor/raygui.h"
#include "../generated/title_layout.h"

namespace {

using TitleAdapter = RGuiAdapter<TitleLayoutState,
                                 &TitleLayout_Init,
                                 &TitleLayout_Render>;
TitleAdapter title_adapter;

} // anonymous namespace

void title_adapter_init() {
    title_adapter.init();
}

void title_adapter_render(entt::registry& reg) {
    title_adapter.render();

#ifndef PLATFORM_WEB
    if (title_adapter.state().ExitButtonPressed) {
        reg.ctx().get<InputState>().quit_requested = true;
    }
#endif

    if (title_adapter.state().SettingsButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Settings;
    }
}
