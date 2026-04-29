#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/ui_state.h"

void ui_navigation_system(entt::registry& reg, float /*dt*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& ui = reg.ctx().get<UIState>();
    ui.has_overlay = (gs.phase == GamePhase::Paused);

    switch (gs.phase) {
        case GamePhase::Title:        ui.active = ActiveScreen::Title; break;
        case GamePhase::LevelSelect:  ui.active = ActiveScreen::LevelSelect; break;
        case GamePhase::Playing:      ui.active = ActiveScreen::Gameplay; break;
        case GamePhase::GameOver:     ui.active = ActiveScreen::GameOver; break;
        case GamePhase::SongComplete: ui.active = ActiveScreen::SongComplete; break;
        case GamePhase::Settings:     ui.active = ActiveScreen::Settings; break;
        case GamePhase::Tutorial:     ui.active = ActiveScreen::Tutorial; break;
        case GamePhase::Paused:       ui.active = ActiveScreen::Paused; break;
    }
}
