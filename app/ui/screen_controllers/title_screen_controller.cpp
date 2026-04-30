// Title screen controller - bridges generated rguilayout output to game systems.

#include "../../components/game_state.h"
#include "../../components/input.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/title_layout.h"

namespace {

using TitleController = RGuiScreenController<TitleLayoutState,
                                              &TitleLayout_Init,
                                              &TitleLayout_Render>;
TitleController title_controller;

bool is_start_tap(const TitleLayoutState& state) {
    if (!IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) return false;
    const Vector2 mouse = GetMousePosition();
    const Rectangle settings_button = {state.Anchor01.x + 632, state.Anchor01.y + 1170, 64, 64};
#ifndef PLATFORM_WEB
    const Rectangle exit_button = {state.Anchor01.x + 260, state.Anchor01.y + 1080, 200, 56};
    if (CheckCollisionPointRec(mouse, exit_button)) return false;
#endif
    if (CheckCollisionPointRec(mouse, settings_button)) return false;
    return true;
}

} // anonymous namespace

void init_title_screen_ui() {
    title_controller.init();
}

void render_title_screen_ui(entt::registry& reg) {
    auto& state = title_controller.state();
    const int saved_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const int saved_label_alignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);

    GuiSetStyle(DEFAULT, TEXT_SIZE, 28);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    title_controller.render();
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, saved_label_alignment);
    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);

#ifndef PLATFORM_WEB
    if (state.ExitButtonPressed) {
        reg.ctx().get<InputState>().quit_requested = true;
    }
#endif

    if (state.SettingsButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Settings;
    }

    if (is_start_tap(state)) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::LevelSelect;
    }
}
