#pragma once
#include <nlohmann/json.hpp>
#include <string>

// Which UI screen the renderer should draw.
// Set by ui_navigation_system, consumed by ui_render_system.
enum class ActiveScreen : uint8_t {
    Title,
    LevelSelect,
    Gameplay,
    GameOver,
    SongComplete,
    Paused,
};

struct UIState {
    nlohmann::json screen;           // primary screen JSON
    nlohmann::json overlay_screen;   // overlay JSON (paused, game over, etc.)
    std::string current;
    std::string base_dir = "content/ui";
    ActiveScreen active = ActiveScreen::Title;
    bool has_overlay = false;

    // Returns true if the screen actually changed (new name + successful load).
    bool load_screen(const std::string& name);
};
