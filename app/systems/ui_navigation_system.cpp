#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/ui_state.h"
#include "../components/ui_element.h"
#include "../components/ui_layout_cache.h"
#include "../constants.h"
#include "../ui/ui_loader.h"
#include <cstring>
#include <raylib.h>

static const char* phase_to_screen_name(GamePhase phase) {
    switch (phase) {
        case GamePhase::Title:        return "title";
        case GamePhase::LevelSelect:  return "level_select";
        case GamePhase::Playing:      return "gameplay";
        case GamePhase::Paused:       return "gameplay";
        case GamePhase::GameOver:     return "game_over";
        case GamePhase::SongComplete: return "song_complete";
    }
    return "title";
}

// Destroy all UI element entities from the previous screen.
static void destroy_ui_elements(entt::registry& reg) {
    auto view = reg.view<UIElementTag>();
    for (auto entity : view) reg.destroy(entity);
}

void ui_navigation_system(entt::registry& reg, float /*dt*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& ui = reg.ctx().get<UIState>();

    const char* screen_name = phase_to_screen_name(gs.phase);
    bool screen_changed = ui_load_screen(ui, screen_name);
    if (screen_changed) {
        build_ui_element_map(ui);
        // Build layout caches for screens with specialized render paths (#322).
        // Pre-multiplied pixel coords eliminate per-frame JSON field access.
        if (std::strcmp(screen_name, "gameplay") == 0) {
            reg.ctx().insert_or_assign(build_hud_layout(ui));
        } else if (std::strcmp(screen_name, "level_select") == 0) {
            reg.ctx().insert_or_assign(build_level_select_layout(ui));
        }
    }
    ui.has_overlay = false;

    // Re-spawn UI elements when screen changes
    if (screen_changed) {
        destroy_ui_elements(reg);
        spawn_ui_elements(reg, ui.screen);
    }

    switch (gs.phase) {
        case GamePhase::Title:        ui.active = ActiveScreen::Title; break;
        case GamePhase::LevelSelect:  ui.active = ActiveScreen::LevelSelect; break;
        case GamePhase::Playing:      ui.active = ActiveScreen::Gameplay; break;
        case GamePhase::GameOver:     ui.active = ActiveScreen::GameOver; break;
        case GamePhase::SongComplete: ui.active = ActiveScreen::SongComplete; break;
        case GamePhase::Paused:
            // Load the overlay JSON once when first entering Paused.
            // Subsequent frames while Paused reuse the cached overlay_screen.
            if (ui.active != ActiveScreen::Paused) {
                ui_load_overlay(ui, "paused");
                reg.ctx().insert_or_assign(build_overlay_layout(ui));
            } else {
                ui.has_overlay = true;
            }
            ui.active = ActiveScreen::Paused;
            break;
    }
}
