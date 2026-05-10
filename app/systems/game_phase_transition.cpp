#include "game_phase_transition.h"

#if defined(EMSCRIPTEN)
#include <emscripten/emscripten.h>
#endif

#include <string>

namespace {

#if defined(EMSCRIPTEN)
const char* game_phase_name(GamePhase phase) {
    switch (phase) {
        case GamePhase::Title:
            return "Title";
        case GamePhase::LevelSelect:
            return "LevelSelect";
        case GamePhase::Playing:
            return "Playing";
        case GamePhase::Paused:
            return "Paused";
        case GamePhase::GameOver:
            return "GameOver";
        case GamePhase::SongComplete:
            return "SongComplete";
        case GamePhase::Settings:
            return "Settings";
        case GamePhase::Tutorial:
            return "Tutorial";
        default:
            return "Unknown";
    }
}

void update_web_title(GamePhase phase) {
    const std::string title = std::string("SHAPESHIFTER [") + game_phase_name(phase) + "]";
    emscripten_set_window_title(title.c_str());
}
#endif

}  // namespace

void enter_phase(GameState& gs, GamePhase next_phase) {
    gs.previous_phase = gs.phase;
    gs.phase = next_phase;
    gs.phase_timer = 0.0f;
#if defined(EMSCRIPTEN)
    update_web_title(gs.phase);
#endif
}
