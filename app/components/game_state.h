#pragma once

#include <cstdint>

enum class GamePhase : uint8_t {
    Title        = 0,
    Playing      = 1,
    Paused       = 2,
    GameOver     = 3,
    SongComplete = 4
};

struct GameState {
    GamePhase phase            = GamePhase::Title;
    GamePhase previous_phase   = GamePhase::Title;
    float     phase_timer      = 0.0f;
    bool      transition_pending = false;
    GamePhase next_phase       = GamePhase::Title;
    float     transition_alpha = 0.0f;
};
