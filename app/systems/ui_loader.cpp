#include "ui_loader.h"
#include "../components/ui_layout_cache.h"
#include "../constants.h"
#include <entt/entt.hpp>
#include <raylib.h>
#include <fstream>
#include <cstdio>

// Supported element types for schema validation.
static const char* const kSupportedTypes[] = {
    "text", "button", "shape", "text_dynamic", "stat_table",
    "shape_button_row", "burnout_meter", "line", "card_list", "button_row",
    nullptr
};

static bool is_supported_type(const std::string& t) {
    for (int i = 0; kSupportedTypes[i]; ++i) {
        if (t == kSupportedTypes[i]) return true;
    }
    return false;
}

std::vector<UIElementDiagnostic> validate_ui_element_types(const nlohmann::json& screen) {
    std::vector<UIElementDiagnostic> out;
    if (!screen.is_object() || !screen.contains("elements")) return out;
    const auto& elements = screen["elements"];
    if (!elements.is_array()) return out;

    int idx = 0;
    for (const auto& el : elements) {
        std::string type_str;
        if (!el.contains("type")) {
            type_str = "<missing>";
        } else if (!el["type"].is_string()) {
            type_str = "<non-string:" + std::string(el["type"].type_name()) + ">";
        } else {
            type_str = el["type"].get<std::string>();
        }

        if (!type_str.empty() && type_str[0] != '<' && !is_supported_type(type_str)) {
            UIElementDiagnostic d;
            d.element_id = el.value("id", "<unknown>");
            d.element_type = type_str;
            d.element_index = idx;
            out.push_back(std::move(d));
        } else if (type_str[0] == '<') {
            // missing or non-string type is always a diagnostic
            UIElementDiagnostic d;
            d.element_id = el.value("id", "<unknown>");
            d.element_type = type_str;
            d.element_index = idx;
            out.push_back(std::move(d));
        }
        ++idx;
    }
    return out;
}

bool validate_overlay_schema(const nlohmann::json& overlay) {
    if (!overlay.is_object()) return false;
    if (!overlay.contains("color")) return false;
    const auto& color = overlay["color"];
    if (!color.is_array()) return false;
    if (color.size() < 3 || color.size() > 4) return false;
    for (const auto& v : color) {
        if (!v.is_number_integer()) return false;
        int n = v.get<int>();
        if (n < 0 || n > 255) return false;
    }
    return true;
}

std::optional<RaylibColor> extract_overlay_color(const nlohmann::json& screen) {
    if (!screen.is_object()) return std::nullopt;
    if (!screen.contains("overlay")) return std::nullopt;
    const auto& overlay = screen["overlay"];
    if (!validate_overlay_schema(overlay)) return std::nullopt;
    const auto& c = overlay["color"];
    uint8_t a = c.size() > 3 ? static_cast<uint8_t>(c[3].get<int>()) : 255;
    return RaylibColor{
        static_cast<uint8_t>(c[0].get<int>()),
        static_cast<uint8_t>(c[1].get<int>()),
        static_cast<uint8_t>(c[2].get<int>()),
        a
    };
}

bool ui_load_screen(UIState& ui, const std::string& name) {
    if (name == ui.current) return false;
    std::string path = ui.base_dir + "/screens/" + name + ".json";
    std::ifstream f(path);
    if (f.is_open()) {
        try {
            ui.screen = nlohmann::json::parse(f);
            ui.current = name;
            return true;
        } catch (const nlohmann::json::exception& e) {
            std::fprintf(stderr, "[WARN] UI screen parse error: %s (%s)\n",
                         path.c_str(), e.what());
        }
    } else {
        std::fprintf(stderr, "[WARN] UI screen not found: %s\n", path.c_str());
    }
    return false;
}

void build_ui_element_map(UIState& ui) {
    ui.element_map.clear();
    if (!ui.screen.contains("elements")) return;
    const auto& elems = ui.screen["elements"];
    for (std::size_t i = 0; i < elems.size(); ++i) {
        std::string id = elems[i].value("id", "");
        if (!id.empty()) {
            ui.element_map[entt::hashed_string::value(id.c_str())] = i;
        }
    }
}

void ui_load_overlay(UIState& ui, const std::string& screen_name) {
    std::string path = ui.base_dir + "/screens/" + screen_name + ".json";
    std::ifstream f(path);
    if (f.is_open()) {
        try {
            ui.overlay_screen = nlohmann::json::parse(f);
            ui.has_overlay = true;
        } catch (const nlohmann::json::exception& e) {
            std::fprintf(stderr, "[WARN] UI overlay parse error: %s (%s)\n",
                         path.c_str(), e.what());
        }
    }
}

UIState load_ui(const std::string& ui_dir) {
    UIState ui;
    ui.base_dir = ui_dir;

    // Note: content/ui/routes.json is intentionally not parsed here.
    // It is a human-authored design reference describing screen transitions;
    // the actual screen graph is driven by GamePhase via phase_to_screen_name()
    // in ui_navigation_system. Loading it would be a per-startup JSON parse
    // with no consumer.

    // Don't pre-load the initial screen here — ui_navigation_system
    // will detect the first phase→screen mapping as a change and both
    // load the JSON *and* spawn the UIElementTag entities.
    return ui;
}

// ── Layout cache builders (#322) ──────────────────────────────────────────────

// Helper: build a raylib Color from a JSON array [r, g, b] or [r, g, b, a].
static Color json_color_rl(const nlohmann::json& arr) {
    uint8_t a = arr.size() > 3 ? arr[3].get<uint8_t>() : 255;
    return {arr[0].get<uint8_t>(), arr[1].get<uint8_t>(),
            arr[2].get<uint8_t>(), a};
}

// Return a pointer into ui.screen["elements"] by hashed id, or nullptr.
static const nlohmann::json* find_layout_el(const UIState& ui, entt::id_type key) {
    auto it = ui.element_map.find(key);
    if (it == ui.element_map.end()) return nullptr;
    if (!ui.screen.contains("elements")) return nullptr;
    const auto& elems = ui.screen["elements"];
    if (it->second >= elems.size()) return nullptr;
    return &elems[it->second];
}

HudLayout build_hud_layout(const UIState& ui) {
    using namespace entt::literals;
    HudLayout layout{};

    const auto* sb_ptr = find_layout_el(ui, "shape_buttons"_hs);
    if (!sb_ptr) return layout;

    try {
        const auto& sb = *sb_ptr;
        layout.btn_w       = sb.at("button_w_n").get<float>() * constants::SCREEN_W_F;
        layout.btn_h       = sb.at("button_h_n").get<float>() * constants::SCREEN_H_F;
        layout.btn_spacing = sb.at("spacing_n").get<float>()  * constants::SCREEN_W_F;
        layout.btn_y       = sb.at("y_n").get<float>()        * constants::SCREEN_H_F;
        layout.active_bg      = json_color_rl(sb.at("active_bg"));
        layout.inactive_bg    = json_color_rl(sb.at("inactive_bg"));
        layout.active_border  = json_color_rl(sb.at("active_border"));
        layout.inactive_border = json_color_rl(sb.at("inactive_border"));
        layout.active_icon    = json_color_rl(sb.at("active_icon"));
        layout.inactive_icon  = json_color_rl(sb.at("inactive_icon"));
        const auto& ar = sb.at("approach_ring");
        layout.approach_ring_max_radius_scale = ar.at("max_radius_scale").get<float>();
        layout.ring_perfect = json_color_rl(ar.at("perfect_color"));
        layout.ring_near    = json_color_rl(ar.at("near_color"));
        layout.ring_far     = json_color_rl(ar.at("far_color"));
    } catch (const nlohmann::json::exception& e) {
        std::fprintf(stderr, "[WARN] build_hud_layout: shape_buttons field error: %s\n",
                     e.what());
        return layout;
    }

    // lane_divider is optional — missing it is not a failure
    const auto* ld_ptr = find_layout_el(ui, "lane_divider"_hs);
    if (ld_ptr) {
        try {
            const auto& ld = *ld_ptr;
            layout.lane_divider_y     = ld.at("y_n").get<float>() * constants::SCREEN_H_F;
            layout.lane_divider_color = json_color_rl(ld.at("color"));
            layout.has_lane_divider   = true;
        } catch (const nlohmann::json::exception& e) {
            std::fprintf(stderr, "[WARN] build_hud_layout: lane_divider field error: %s\n",
                         e.what());
        }
    }

    layout.valid = true;
    return layout;
}

LevelSelectLayout build_level_select_layout(const UIState& ui) {
    using namespace entt::literals;
    LevelSelectLayout layout{};

    const auto* sc_ptr = find_layout_el(ui, "song_cards"_hs);
    if (!sc_ptr) return layout;

    try {
        const auto& sc = *sc_ptr;
        layout.card_x            = sc.at("x_n").get<float>()          * constants::SCREEN_W_F;
        layout.start_y           = sc.at("start_y_n").get<float>()    * constants::SCREEN_H_F;
        layout.card_w            = sc.at("card_w_n").get<float>()      * constants::SCREEN_W_F;
        layout.card_h            = sc.at("card_h_n").get<float>()      * constants::SCREEN_H_F;
        layout.card_gap          = sc.at("card_gap_n").get<float>()    * constants::SCREEN_H_F;
        layout.card_corner_radius = sc.value("corner_radius", 0.1f);
        layout.selected_bg       = json_color_rl(sc.at("selected_bg"));
        layout.unselected_bg     = json_color_rl(sc.at("unselected_bg"));
        layout.selected_border   = json_color_rl(sc.at("selected_border"));
        layout.unselected_border = json_color_rl(sc.at("unselected_border"));
        layout.title_offset_x    = sc.at("title_offset_x_n").get<float>() * constants::SCREEN_W_F;
        layout.title_offset_y    = sc.at("title_offset_y_n").get<float>() * constants::SCREEN_H_F;
    } catch (const nlohmann::json::exception& e) {
        std::fprintf(stderr, "[WARN] build_level_select_layout: song_cards field error: %s\n",
                     e.what());
        return layout;
    }

    const auto* db_ptr = find_layout_el(ui, "difficulty_buttons"_hs);
    if (!db_ptr) return layout;

    try {
        const auto& db = *db_ptr;
        layout.diff_y_offset      = db.at("y_offset_n").get<float>()  * constants::SCREEN_H_F;
        layout.dx_start           = db.at("x_start_n").get<float>()   * constants::SCREEN_W_F;
        layout.diff_btn_w         = db.at("button_w_n").get<float>()  * constants::SCREEN_W_F;
        layout.diff_btn_h         = db.at("button_h_n").get<float>()  * constants::SCREEN_H_F;
        layout.diff_btn_gap       = db.at("button_gap_n").get<float>() * constants::SCREEN_W_F;
        layout.diff_corner_radius = db.value("corner_radius", 0.2f);
        layout.diff_active_bg     = json_color_rl(db.at("active_bg"));
        layout.diff_active_border = json_color_rl(db.at("active_border"));
        layout.diff_active_text   = json_color_rl(db.at("active_text"));
        layout.diff_inactive_bg   = json_color_rl(db.at("inactive_bg"));
        layout.diff_inactive_border = json_color_rl(db.at("inactive_border"));
        layout.diff_inactive_text = json_color_rl(db.at("inactive_text"));
    } catch (const nlohmann::json::exception& e) {
        std::fprintf(stderr,
                     "[WARN] build_level_select_layout: difficulty_buttons field error: %s\n",
                     e.what());
        return layout;
    }

    layout.valid = true;
    return layout;
}
