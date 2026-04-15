#pragma once
#include <nlohmann/json.hpp>
#include <string>

struct UIState {
    nlohmann::json routes;
    nlohmann::json screen;
    std::string current;
    std::string base_dir = "content/ui";

    void load_screen(const std::string& name);
};
