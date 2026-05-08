#include "input_routing.h"
#include "../components/game_state.h"
#include "../components/input_events.h"
#include "../components/haptics.h"
#include "../util/settings.h"

bool game_state_handle_end_screen_press(entt::registry& reg, const ButtonPressEvent& evt) {
    auto& gs = reg.ctx().get<GameState>();
    if ((gs.phase != GamePhase::GameOver && gs.phase != GamePhase::SongComplete) ||
        gs.phase_timer <= 0.4f) {
        return false;
    }

    auto* haptics = reg.ctx().find<HapticQueue>();
    auto* settings = reg.ctx().find<SettingsState>();
    if (haptics && (!settings || settings->haptics_enabled) &&
        haptics->count < HapticQueue::MAX_QUEUED) {
        haptics->queue[haptics->count++] = evt.menu_action == MenuActionKind::Restart
                                            ? HapticEvent::RetryTap
                                            : HapticEvent::UIButtonTap;
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
