#include "level_select_controller.h"
#include "../components/game_state.h"
#include "../util/level_content_config.h"
#include "../systems/input_events.h"

namespace {
// Shared gate for level-select menu presses: only fire on the level-select
// phase and after the entry-debounce window. Mirrors the pre-#1277 guard
// inside the former `level_select_handle_press_menu` switch.
bool level_select_press_gate(entt::registry& reg) {
    if (!reg.ctx().contains<GamePhaseLevelSelectTag>()) return false;
    return reg.ctx().get<GameState>().phase_timer >= 0.05f;
}

// Gate for the directional handlers: ignore presses outside the
// level-select phase. No debounce — directional input was never gated on
// the entry timer (only the confirm/select-press paths were).
bool level_select_dir_gate(entt::registry& reg) {
    return reg.ctx().contains<GamePhaseLevelSelectTag>();
}

void step_level(entt::registry& reg, int delta) {
    auto& lss = reg.ctx().get<LevelSelectState>();
    const int count = static_cast<int>(content_config::LEVEL_COUNT);
    lss.selected_level = (lss.selected_level + delta + count) % count;
}

void step_difficulty(entt::registry& reg, int delta) {
    auto& lss = reg.ctx().get<LevelSelectState>();
    const int count = static_cast<int>(content_config::DIFFICULTY_COUNT);
    lss.selected_difficulty = (lss.selected_difficulty + delta + count) % count;
}
} // namespace

void level_select_handle_go_up(entt::registry& reg, const GoUpEvent&) {
    if (!level_select_dir_gate(reg)) return;
    step_level(reg, -1);
}

void level_select_handle_go_down(entt::registry& reg, const GoDownEvent&) {
    if (!level_select_dir_gate(reg)) return;
    step_level(reg, +1);
}

void level_select_handle_go_left(entt::registry& reg, const GoLeftEvent&) {
    if (!level_select_dir_gate(reg)) return;
    step_difficulty(reg, -1);
}

void level_select_handle_go_right(entt::registry& reg, const GoRightEvent&) {
    if (!level_select_dir_gate(reg)) return;
    step_difficulty(reg, +1);
}

void level_select_handle_confirm(entt::registry& reg, const MenuConfirmEvent&) {
    if (!level_select_press_gate(reg)) return;
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});
}

void level_select_handle_select_level(entt::registry& reg, const MenuSelectLevelEvent& evt) {
    if (!level_select_press_gate(reg)) return;
    if (!content_config::is_valid_level_index(evt.index)) return;
    reg.ctx().get<LevelSelectState>().selected_level = evt.index;
}

void level_select_handle_select_diff(entt::registry& reg, const MenuSelectDiffEvent& evt) {
    if (!level_select_press_gate(reg)) return;
    if (!content_config::is_valid_difficulty_index(evt.index)) return;
    reg.ctx().get<LevelSelectState>().selected_difficulty = evt.index;
}
