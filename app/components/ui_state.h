#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <entt/entt.hpp>

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

// Pure data component. File I/O is performed by ui_loader free functions.
struct UIState {
    nlohmann::json screen;           // primary screen JSON
    nlohmann::json overlay_screen;   // overlay JSON (paused, game over, etc.)
    std::string current;
    std::string base_dir = "content/ui";
    ActiveScreen active = ActiveScreen::Title;
    bool has_overlay = false;

    // Pre-computed element lookup built at screen-load time (#312).
    // Maps entt::hashed_string value of each element's "id" field to its
    // index in screen["elements"].  O(1) lookups replace the per-frame
    // linear scan performed by the render system's find_el helper.
    std::unordered_map<entt::id_type, std::size_t> element_map;
};
