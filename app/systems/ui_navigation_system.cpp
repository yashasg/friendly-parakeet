#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/ui_state.h"
#include <fstream>

static const char* phase_to_screen_name(GamePhase phase) {
    switch (phase) {
        case GamePhase::Title:        return "title";
        case GamePhase::LevelSelect:  return "level_select";
        case GamePhase::Playing:      return "gameplay";
        case GamePhase::Paused:       return "gameplay";  // HUD uses gameplay screen
        case GamePhase::GameOver:     return "game_over";
        case GamePhase::SongComplete: return "song_complete";
    }
    return "title";
}

void ui_navigation_system(entt::registry& reg, float /*dt*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& ui = reg.ctx().get<UIState>();

    // Load the primary screen for the current phase
    ui.load_screen(phase_to_screen_name(gs.phase));
    ui.has_overlay = false;

    switch (gs.phase) {
        case GamePhase::Title:
            ui.active = ActiveScreen::Title;
            break;
        case GamePhase::LevelSelect:
            ui.active = ActiveScreen::LevelSelect;
            break;
        case GamePhase::Playing:
            ui.active = ActiveScreen::Gameplay;
            break;
        case GamePhase::Paused:
            ui.active = ActiveScreen::Paused;
            ui.has_overlay = true;
            // Load paused overlay
            {
                std::string path = ui.base_dir + "/screens/paused.json";
                std::ifstream f(path);
                if (f.is_open())
                    ui.overlay_screen = nlohmann::json::parse(f);
            }
            break;
        case GamePhase::GameOver:
            ui.active = ActiveScreen::GameOver;
            break;
        case GamePhase::SongComplete:
            ui.active = ActiveScreen::SongComplete;
            break;
    }
}
