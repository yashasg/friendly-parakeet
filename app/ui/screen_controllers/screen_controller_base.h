// Shared infrastructure for screen controllers.
// Provides base class and helper utilities for RGui-based screen implementations.

#ifndef SCREEN_CONTROLLER_BASE_H
#define SCREEN_CONTROLLER_BASE_H

#include <entt/entt.hpp>
#include <raylib.h>

#include "../../components/game_state.h"

// Template controller: manages layout state lifecycle and provides uniform render interface.
// LayoutState: generated state struct (e.g., GameOverLayoutState)
// InitFunc:    pointer to generated Init function
// RenderFunc:  pointer to generated Render function
template<typename LayoutState, auto InitFunc, auto RenderFunc>
class RGuiScreenController {
    LayoutState state_;
    bool initialized_ = false;

public:
    void init() {
        if (!initialized_) {
            state_ = InitFunc();
            initialized_ = true;
        }
    }

    void render() {
        if (!initialized_) {
            init();
        }
        RenderFunc(&state_);
    }

    LayoutState& state() { return state_; }
    const LayoutState& state() const { return state_; }
};

// Helper: construct Rectangle with anchor-relative offsets.
inline Rectangle offset_rect(Vector2 anchor, float x, float y, float w, float h) {
    return (Rectangle){anchor.x + x, anchor.y + y, w, h};
}

// Generic end-screen choice dispatcher. Used by game_over and song_complete controllers.
// LayoutState must have RestartButtonPressed, LevelSelectButtonPressed, MenuButtonPressed fields.
template<typename LayoutState>
inline void dispatch_end_screen_choice(GameState& gs, const LayoutState& state) {
    if (state.RestartButtonPressed)
        gs.end_choice = EndScreenChoice::Restart;
    else if (state.LevelSelectButtonPressed)
        gs.end_choice = EndScreenChoice::LevelSelect;
    else if (state.MenuButtonPressed)
        gs.end_choice = EndScreenChoice::MainMenu;
}

#endif // SCREEN_CONTROLLER_BASE_H
