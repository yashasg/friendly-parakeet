#include "ui_loader.h"
#include <fstream>
#include <cstdio>

bool UIState::load_screen(const std::string& name) {
    if (name == current) return false;
    std::string path = base_dir + "/screens/" + name + ".json";
    std::ifstream f(path);
    if (f.is_open()) {
        try {
            screen = nlohmann::json::parse(f);
            current = name;
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

UIState load_ui(const std::string& ui_dir) {
    UIState ui;
    ui.base_dir = ui_dir;

    std::string routes_path = ui_dir + "/routes.json";
    std::ifstream f(routes_path);
    if (f.is_open()) {
        ui.routes = nlohmann::json::parse(f);
    } else {
        std::fprintf(stderr, "[WARN] UI routes not found: %s\n", routes_path.c_str());
    }

    // Don't pre-load the initial screen here — ui_navigation_system
    // will detect the first phase→screen mapping as a change and both
    // load the JSON *and* spawn the UIElementTag entities.
    return ui;
}
