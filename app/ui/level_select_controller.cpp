#include "level_select_controller.h"
#include "../components/game_state.h"
#include "../components/input_events.h"

void level_select_handle_go(entt::registry& reg, const GoEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::LevelSelect) return;

    auto& lss = reg.ctx().get<LevelSelectState>();
    if (evt.dir == Direction::Up) {
        lss.selected_level = (lss.selected_level - 1 + LevelSelectState::LEVEL_COUNT) % LevelSelectState::LEVEL_COUNT;
    } else if (evt.dir == Direction::Down) {
        lss.selected_level = (lss.selected_level + 1) % LevelSelectState::LEVEL_COUNT;
    } else if (evt.dir == Direction::Left) {
        lss.selected_difficulty = (lss.selected_difficulty - 1 + LevelSelectState::DIFFICULTY_COUNT) % LevelSelectState::DIFFICULTY_COUNT;
    } else if (evt.dir == Direction::Right) {
        lss.selected_difficulty = (lss.selected_difficulty + 1) % LevelSelectState::DIFFICULTY_COUNT;
    }
}

void level_select_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase != GamePhase::LevelSelect) return;
    if (gs.phase_timer < 0.05f) return;
    if (evt.kind != ButtonPressKind::Menu) return;

    auto& lss = reg.ctx().get<LevelSelectState>();

    switch (evt.menu_action) {
        case MenuActionKind::Confirm:
            lss.confirmed = true;
            break;
        case MenuActionKind::SelectLevel:
            lss.selected_level = evt.menu_index;
            break;
        case MenuActionKind::SelectDiff:
            lss.selected_difficulty = evt.menu_index;
            break;
        default: break;
    }
}
