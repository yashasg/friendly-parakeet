#include "input_routing.h"
#include "../components/game_state.h"
#include "../components/input_events.h"
#include "../components/haptics.h"
#include "../util/haptic_queue.h"
#include "../util/settings.h"

bool game_state_handle_end_screen_press(entt::registry& reg, const ButtonPressEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if ((gs.phase != GamePhase::GameOver && gs.phase != GamePhase::SongComplete) ||
        gs.phase_timer <= 0.4f) {
        return false;
    }

    auto* hq = reg.ctx().find<HapticQueue>();
    auto* st = reg.ctx().find<SettingsState>();
    if (hq) {
        const bool haptics_on = !st || st->haptics_enabled;
        if (evt.menu_action == MenuActionKind::Restart) {
            haptic_push(*hq, haptics_on, HapticEvent::RetryTap);
        } else {
            haptic_push(*hq, haptics_on, HapticEvent::UIButtonTap);
        }
    }

    if (evt.menu_action == MenuActionKind::Restart) {
        gs.end_choice = EndScreenChoice::Restart;
    } else if (evt.menu_action == MenuActionKind::GoLevelSelect) {
        gs.end_choice = EndScreenChoice::LevelSelect;
    } else if (evt.menu_action == MenuActionKind::GoMainMenu) {
        gs.end_choice = EndScreenChoice::MainMenu;
    }
    return true;
}
