// Title screen controller - bridges generated rguilayout output to game systems.

#include "../../components/game_state.h"
#include "../../components/input.h"
#include "../../components/rendering.h"
#include "../../input/pointer_input.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/title_layout.h"

namespace {

using TitleController = RGuiScreenController<TitleLayoutState,
                                              &TitleLayout_Init,
                                              &TitleLayout_Render>;
TitleController title_controller;

bool read_title_pointer_release(const entt::registry& reg, Vector2& pointer) {
    const auto& input = reg.ctx().get<InputState>();
    if (pointer_release_position(input, pointer)) return true;
    if (input.touch_down) {
        pointer = {input.start_x, input.start_y};
        return true;
    }

    // Title activation is tolerant to browser stacks that miss release edges:
    // accept either edge from raylib and map to virtual UI coordinates.
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const auto& st = reg.ctx().get<ScreenTransform>();
        const Vector2 raw = GetMousePosition();
        pointer = {(raw.x - st.offset_x) / st.scale, (raw.y - st.offset_y) / st.scale};
        return true;
    }
    return false;
}

bool is_start_tap(const entt::registry& reg, const TitleLayoutState& state) {
    Vector2 pointer = {};
    if (!read_title_pointer_release(reg, pointer)) return false;
    const Rectangle settings_button = {state.Anchor01.x + 632, state.Anchor01.y + 1170, 64, 64};
#ifndef PLATFORM_WEB
    const Rectangle exit_button = {state.Anchor01.x + 260, state.Anchor01.y + 1080, 200, 56};
    if (CheckCollisionPointRec(pointer, exit_button)) return false;
#endif
    if (CheckCollisionPointRec(pointer, settings_button)) return false;
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

    if (is_start_tap(reg, state)) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::LevelSelect;
    }
}
