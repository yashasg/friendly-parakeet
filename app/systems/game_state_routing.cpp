#include "input_routing.h"
#include "../components/game_state.h"
#include "../components/input_events.h"
#include "../components/registry_context.h"
#include "../util/haptic_queue.h"

namespace {

int wrap_index(const int value, const int count) {
    if (count <= 0) return 0;
    return (value % count + count) % count;
}

void update_level_select_cursor(LevelSelectState& lss, const Direction dir) {
    switch (dir) {
        case Direction::Up:
            lss.selected_level = wrap_index(lss.selected_level - 1, LevelSelectState::LEVEL_COUNT);
            return;
        case Direction::Down:
            lss.selected_level = wrap_index(lss.selected_level + 1, LevelSelectState::LEVEL_COUNT);
            return;
        case Direction::Left:
            lss.selected_difficulty =
                wrap_index(lss.selected_difficulty - 1, LevelSelectState::DIFFICULTY_COUNT);
            return;
        case Direction::Right:
            lss.selected_difficulty =
                wrap_index(lss.selected_difficulty + 1, LevelSelectState::DIFFICULTY_COUNT);
            return;
    }
}

EndScreenChoice end_choice_for_menu_action(const MenuActionKind action) {
    switch (action) {
        case MenuActionKind::Restart:
            return EndScreenChoice::Restart;
        case MenuActionKind::GoLevelSelect:
            return EndScreenChoice::LevelSelect;
        case MenuActionKind::GoMainMenu:
            return EndScreenChoice::MainMenu;
        default:
            return EndScreenChoice::None;
    }
}

void handle_end_screen_press(entt::registry& reg, GameState& gs, const ButtonPressEvent& evt) {
    if (!is_end_screen_phase(gs.phase) ||
        gs.phase_timer <= game_phase_timing::END_SCREEN_INPUT_DELAY_SEC) {
        return;
    }

    const MenuActionKind action =
        (evt.menu_action == MenuActionKind::Confirm) ? MenuActionKind::Restart : evt.menu_action;

    haptic_feedback(reg, action == MenuActionKind::Restart
                             ? HapticEvent::RetryTap
                             : HapticEvent::UIButtonTap);
    gs.end_choice = end_choice_for_menu_action(action);
}

void handle_title_press(entt::registry& reg, GameState& gs, const ButtonPressEvent& evt) {
    haptic_feedback(reg, HapticEvent::UIButtonTap);
    if (evt.menu_action == MenuActionKind::Exit) {
#ifndef PLATFORM_WEB
        registry_ctx_get<InputState>(reg).quit_requested = true;
#endif
    } else if (evt.menu_action == MenuActionKind::Confirm) {
        request_phase_transition(gs, GamePhase::LevelSelect);
    }
}

}  // namespace

void game_state_handle_go(entt::registry& reg, const GoEvent& /*evt*/) {
    auto& gs = registry_ctx_get<GameState>(reg);
    if (gs.phase != GamePhase::Paused) return;
    enter_phase(gs, GamePhase::Playing);
}

void game_state_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    if (evt.kind != ButtonPressKind::Menu) return;  // ignore shape-button events
    auto& gs = registry_ctx_get<GameState>(reg);

    switch (gs.phase) {
        case GamePhase::Title:
            handle_title_press(reg, gs, evt);
            return;

        case GamePhase::GameOver:
        case GamePhase::SongComplete:
            handle_end_screen_press(reg, gs, evt);
            return;

        case GamePhase::Settings:
        case GamePhase::Tutorial:
            if (evt.menu_action == MenuActionKind::Confirm) {
                request_phase_transition(gs, GamePhase::Title);
            }
            return;

        case GamePhase::Paused:
            enter_phase(gs, GamePhase::Playing);
            return;

        default:
            return;
    }
}

void level_select_handle_go(entt::registry& reg, const GoEvent& evt) {
    auto& gs = registry_ctx_get<GameState>(reg);
    if (gs.phase != GamePhase::LevelSelect) return;

    auto& lss = registry_ctx_get<LevelSelectState>(reg);
    update_level_select_cursor(lss, evt.dir);
}

void level_select_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    auto& gs = registry_ctx_get<GameState>(reg);
    if (gs.phase != GamePhase::LevelSelect) return;
    if (gs.phase_timer < game_phase_timing::LEVEL_SELECT_INPUT_DELAY_SEC) return;
    if (evt.kind != ButtonPressKind::Menu) return;

    auto& lss = registry_ctx_get<LevelSelectState>(reg);
    switch (evt.menu_action) {
        case MenuActionKind::Confirm:
            lss.confirmed = true;
            break;
        case MenuActionKind::SelectLevel:
            lss.selected_level = wrap_index(evt.menu_index, LevelSelectState::LEVEL_COUNT);
            break;
        case MenuActionKind::SelectDiff:
            lss.selected_difficulty = wrap_index(evt.menu_index, LevelSelectState::DIFFICULTY_COUNT);
            break;
        default:
            break;
    }
}
