#pragma once
#include "components/ui_state.h"
#include <optional>
#include <string>
#include <vector>

// --- Structural schema helpers (used by tests) ---

struct UIElementDiagnostic {
    std::string element_id;
    std::string element_type;
    int element_index = 0;
};

std::vector<UIElementDiagnostic> validate_ui_element_types(const nlohmann::json& screen);
bool validate_overlay_schema(const nlohmann::json& overlay);

struct RaylibColor { uint8_t r, g, b, a; };
std::optional<RaylibColor> extract_overlay_color(const nlohmann::json& screen);

// --- Loader free functions ---

// Build a default UIState with base_dir set. Screens are loaded lazily.
UIState load_ui(const std::string& ui_dir = "content/ui");

// Load the named screen JSON into ui.screen. Returns true if the screen
// actually changed (new name + successful parse). Pure file I/O; no ECS access.
bool ui_load_screen(UIState& ui, const std::string& name);

// Load the overlay screen JSON into ui.overlay_screen and set ui.has_overlay.
// Pure file I/O; no ECS access.
void ui_load_overlay(UIState& ui, const std::string& screen_name);
