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
} // namespace

void level_select_handle_go(entt::registry& reg, const GoEvent& evt) {
    if (!reg.ctx().contains<GamePhaseLevelSelectTag>()) return;

    auto& lss = reg.ctx().get<LevelSelectState>();
    if (evt.dir == Direction::Up) {
        lss.selected_level = (lss.selected_level - 1 + content_config::LEVEL_COUNT) % content_config::LEVEL_COUNT;
    } else if (evt.dir == Direction::Down) {
        lss.selected_level = (lss.selected_level + 1) % content_config::LEVEL_COUNT;
    } else if (evt.dir == Direction::Left) {
        lss.selected_difficulty = (lss.selected_difficulty - 1 + content_config::DIFFICULTY_COUNT) % content_config::DIFFICULTY_COUNT;
    } else if (evt.dir == Direction::Right) {
        lss.selected_difficulty = (lss.selected_difficulty + 1) % content_config::DIFFICULTY_COUNT;
    }
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
