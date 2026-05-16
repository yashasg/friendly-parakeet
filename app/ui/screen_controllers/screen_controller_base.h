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

template<typename Controller>
inline Controller& screen_controller(entt::registry& reg) {
    if (auto* existing = reg.ctx().find<Controller>()) return *existing;
    auto& created = reg.ctx().emplace<Controller>();
    created.init();
    return created;
}

// Helper: construct Rectangle with anchor-relative offsets.
inline Rectangle offset_rect(Vector2 anchor, float x, float y, float w, float h) {
    return Rectangle{anchor.x + x, anchor.y + y, w, h};
}

// Generic end-screen choice dispatcher. Used by game_over and song_complete controllers.
// LayoutState must have RestartButtonPressed, LevelSelectButtonPressed, MenuButtonPressed fields.
// Each branch emplaces the corresponding zero-column ctx table; absence of all three
// is the "no choice yet" state. game_state_end_screen_system consumes the tag once
// the per-phase input delay has elapsed.
template<typename LayoutState>
inline void dispatch_end_screen_choice(entt::registry& reg, const LayoutState& state) {
    if (state.RestartButtonPressed)
        reg.ctx().insert_or_assign(EndChoiceRestart{});
    else if (state.LevelSelectButtonPressed)
        reg.ctx().insert_or_assign(EndChoiceLevelSelect{});
    else if (state.MenuButtonPressed)
        reg.ctx().insert_or_assign(EndChoiceMainMenu{});
}

#endif // SCREEN_CONTROLLER_BASE_H
