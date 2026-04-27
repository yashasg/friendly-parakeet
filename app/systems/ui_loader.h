#pragma once
#include "components/ui_state.h"
#include "components/ui_layout_cache.h"
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

// Build ui.element_map from the current ui.screen["elements"] array.
// Must be called after every successful ui_load_screen(). O(N) build cost
// amortised at screen-load time; subsequent find_el calls are O(1).
void build_ui_element_map(UIState& ui);

// Load the overlay screen JSON into ui.overlay_screen and set ui.has_overlay.
// Pure file I/O; no ECS access.
void ui_load_overlay(UIState& ui, const std::string& screen_name);

// --- Layout cache builders (#322) ---
// Call after build_ui_element_map() when a screen change is detected.
// Reads stable layout constants from ui.screen once; returns a POD struct
// ready to store in reg.ctx().  All coordinate fields are pre-multiplied by
// SCREEN_W / SCREEN_H so the render path does zero JSON access per frame.

// Build HudLayout from the current "gameplay" screen JSON.
// Returns HudLayout{.valid=false} if required elements are absent.
HudLayout build_hud_layout(const UIState& ui);

// Build LevelSelectLayout from the current "level_select" screen JSON.
// Returns LevelSelectLayout{.valid=false} if required elements are absent.
LevelSelectLayout build_level_select_layout(const UIState& ui);

// Build OverlayLayout from ui.overlay_screen using extract_overlay_color().
// Returns OverlayLayout{.valid=false} when overlay_screen is absent or schema is invalid.
OverlayLayout build_overlay_layout(const UIState& ui);
