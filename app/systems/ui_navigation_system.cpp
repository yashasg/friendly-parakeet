#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/ui_state.h"
#include "../components/ui_element.h"
#include "../components/transform.h"
#include "../constants.h"
#include "ui_loader.h"
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

static Color json_color(const nlohmann::json& arr) {
    if (!arr.is_array() || arr.size() < 3) return WHITE;
    uint8_t a = arr.size() > 3 ? arr[3].get<uint8_t>() : 255;
    return {arr[0].get<uint8_t>(), arr[1].get<uint8_t>(),
            arr[2].get<uint8_t>(), a};
}

static FontSize json_font(const std::string& s) {
    if (s == "large") return FontSize::Large;
    if (s == "small") return FontSize::Small;
    return FontSize::Medium;
}

static TextAlign json_align(const std::string& s) {
    if (s == "center") return TextAlign::Center;
    if (s == "right")  return TextAlign::Right;
    return TextAlign::Left;
}

static Shape json_shape(const std::string& s) {
    if (s == "square")   return Shape::Square;
    if (s == "triangle") return Shape::Triangle;
    return Shape::Circle;
}

// Destroy all UI element entities from the previous screen.
static void destroy_ui_elements(entt::registry& reg) {
    auto view = reg.view<UIElementTag>();
    for (auto entity : view) reg.destroy(entity);
}

// Spawn UI element entities from JSON screen definition.
// Returns true if this element should be skipped on the current build platform.
// Mirrors the runtime check used by render_button() in ui_render_system.cpp.
static bool skip_for_platform(const nlohmann::json& el) {
    if (!el.contains("platform_only")) return false;
    auto plat = el.value("platform_only", "");
    #ifdef PLATFORM_WEB
    return plat == "desktop";
    #else
    return plat == "web";
    #endif
}

static void spawn_ui_elements(entt::registry& reg, const nlohmann::json& screen) {
    if (!screen.contains("elements")) return;

    for (auto& el : screen["elements"]) {
        auto type = el.value("type", "");
        if (skip_for_platform(el)) continue;
        auto e = reg.create();
        reg.emplace<UIElementTag>(e);

        float px = el.value("x_n", 0.0f) * constants::SCREEN_W;
        float py = el.value("y_n", 0.0f) * constants::SCREEN_H;
        reg.emplace<Position>(e, px, py);

        if (type == "text" || type == "text_dynamic") {
            UIText t{};
            auto s = el.value("text", "");
            std::snprintf(t.text, sizeof(t.text), "%s", s.c_str());
            t.font_size = json_font(el.value("font_size", "medium"));
            t.align = json_align(el.value("align", "left"));
            // Dynamic text elements may omit `color` (resolved at draw
            // time) but a sensible default keeps the layout legible.
            if (el.contains("color")) {
                t.color = json_color(el["color"]);
            } else {
                t.color = {255, 255, 255, 255};
            }
            reg.emplace<UIText>(e, t);

            if (type == "text_dynamic" && el.contains("source")) {
                UIDynamicText dt{};
                auto src = el.value("source", "");
                auto fmt = el.value("format", "");
                std::snprintf(dt.source, sizeof(dt.source), "%s", src.c_str());
                std::snprintf(dt.format, sizeof(dt.format), "%s", fmt.c_str());
                reg.emplace<UIDynamicText>(e, dt);
            }

            if (el.contains("animation")) {
                UIAnimation anim{};
                anim.speed = el["animation"]["speed"].get<float>();
                anim.alpha_min = el["animation"]["alpha_range"][0].get<uint8_t>();
                anim.alpha_max = el["animation"]["alpha_range"][1].get<uint8_t>();
                reg.emplace<UIAnimation>(e, anim);
            }
        } else if (type == "button") {
            UIButton btn{};
            auto s = el.value("text", "");
            std::snprintf(btn.text, sizeof(btn.text), "%s", s.c_str());
            btn.font_size = json_font(el.value("font_size", "small"));
            btn.w = el.value("w_n", 0.0f) * constants::SCREEN_W;
            btn.h = el.value("h_n", 0.0f) * constants::SCREEN_H;
            btn.corner_radius = el.value("corner_radius", 0.2f);
            btn.bg = json_color(el["bg_color"]);
            btn.border = json_color(el["border_color"]);
            btn.text_color = json_color(el["text_color"]);
            reg.emplace<UIButton>(e, btn);

            // Optional: bind the button face text to a runtime source
            // (e.g. "HAPTICS: ON/OFF") so the control surface always
            // reflects current state.
            if (el.contains("text_source")) {
                UIDynamicText dt{};
                auto src = el.value("text_source", "");
                auto fmt = el.value("format", "");
                std::snprintf(dt.source, sizeof(dt.source), "%s", src.c_str());
                std::snprintf(dt.format, sizeof(dt.format), "%s", fmt.c_str());
                reg.emplace<UIDynamicText>(e, dt);
            }

            if (el.contains("animation")) {
                UIAnimation anim{};
                anim.speed = el["animation"]["speed"].get<float>();
                anim.alpha_min = el["animation"]["alpha_range"][0].get<uint8_t>();
                anim.alpha_max = el["animation"]["alpha_range"][1].get<uint8_t>();
                reg.emplace<UIAnimation>(e, anim);
            }
        } else if (type == "shape") {
            UIShape sh{};
            sh.shape = json_shape(el.value("shape", "circle"));
            sh.size = el.value("size_n", 0.0f) * constants::SCREEN_W;
            sh.color = json_color(el["color"]);
            reg.emplace<UIShape>(e, sh);
        }
    }
}

void ui_navigation_system(entt::registry& reg, float /*dt*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& ui = reg.ctx().get<UIState>();

    const char* screen_name = phase_to_screen_name(gs.phase);
    bool screen_changed = ui_load_screen(ui, screen_name);
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
            } else {
                ui.has_overlay = true;
            }
            ui.active = ActiveScreen::Paused;
            break;
    }
}
