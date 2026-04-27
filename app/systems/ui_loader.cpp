#include "ui_loader.h"
#include <entt/entt.hpp>
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
