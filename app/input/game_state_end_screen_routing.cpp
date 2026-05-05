#include "input_routing.h"
#include "../components/game_state.h"
#include "../components/input_events.h"
#include "../util/haptic_queue.h"

bool game_state_handle_end_screen_press(entt::registry& reg, const ButtonPressEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if ((gs.phase != GamePhase::GameOver && gs.phase != GamePhase::SongComplete) ||
        gs.phase_timer <= 0.4f) {
        return false;
    }

    const MenuActionKind action =
        (evt.menu_action == MenuActionKind::Confirm) ? MenuActionKind::Restart : evt.menu_action;

    haptic_feedback(reg, action == MenuActionKind::Restart
                             ? HapticEvent::RetryTap
                             : HapticEvent::UIButtonTap);

    if (action == MenuActionKind::Restart) {
        gs.end_choice = EndScreenChoice::Restart;
    } else if (action == MenuActionKind::GoLevelSelect) {
        gs.end_choice = EndScreenChoice::LevelSelect;
    } else if (action == MenuActionKind::GoMainMenu) {
        gs.end_choice = EndScreenChoice::MainMenu;
    } else {
        return false;
    }
    return true;
}
