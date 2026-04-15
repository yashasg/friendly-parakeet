#include "ui_loader.h"
#include <fstream>
#include <cstdio>

void UIState::load_screen(const std::string& name) {
    if (name == current) return;
    std::string path = base_dir + "/screens/" + name + ".json";
    std::ifstream f(path);
    if (f.is_open()) {
        screen = nlohmann::json::parse(f);
        current = name;
    } else {
        std::fprintf(stderr, "[WARN] UI screen not found: %s\n", path.c_str());
    }
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

    auto initial = ui.routes.value("initial_screen", "title");
    ui.load_screen(initial);
    return ui;
}
