#pragma once

// Which UI screen the renderer should draw.
// Set by ui_navigation_system, consumed by ui_render_system.
enum class ActiveScreen : uint8_t {
    Title,
    LevelSelect,
    Gameplay,
    GameOver,
    SongComplete,
    Paused,
    Settings,
    Tutorial,
};

// Runtime UI state is now owned by raygui controllers.
struct UIState {
    ActiveScreen active = ActiveScreen::Title;
    bool has_overlay = false;
};
