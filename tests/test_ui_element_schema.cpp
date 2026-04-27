#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "systems/ui_loader.h"
#include "components/ui_state.h"
#include <entt/entt.hpp>
#include <fstream>
#include <string>

TEST_CASE("UI element schema: validate_ui_element_types rejects missing elements", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: validate_ui_element_types accepts empty elements array", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["elements"] = nlohmann::json::array();
    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: validate_ui_element_types accepts supported types", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["elements"] = nlohmann::json::array();

    std::vector<std::string> supported = {
        "text", "button", "shape", "text_dynamic", "stat_table",
        "shape_button_row", "burnout_meter", "line",
        "card_list", "button_row"
    };

    for (const auto& type : supported) {
        nlohmann::json el = nlohmann::json::object();
        el["type"] = type;
        el["id"] = "test_" + type;
        screen["elements"].push_back(el);
    }

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: validate_ui_element_types accepts stat_table", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["elements"] = nlohmann::json::array();

    nlohmann::json el = nlohmann::json::object();
    el["type"] = "stat_table";
    el["id"] = "timing_stats";
    screen["elements"].push_back(el);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: validate_ui_element_types reports multiple unsupported", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["elements"] = nlohmann::json::array();

    nlohmann::json el1 = nlohmann::json::object();
    el1["type"] = "unknown_type_1";
    el1["id"] = "bad_element_1";
    screen["elements"].push_back(el1);

    nlohmann::json el2 = nlohmann::json::object();
    el2["type"] = "text";
    screen["elements"].push_back(el2);

    nlohmann::json el3 = nlohmann::json::object();
    el3["type"] = "unknown_type_2";
    screen["elements"].push_back(el3);

    auto diagnostics = validate_ui_element_types(screen);
    REQUIRE(diagnostics.size() == 2);

    CHECK(diagnostics[0].element_id == "bad_element_1");
    CHECK(diagnostics[0].element_type == "unknown_type_1");
    CHECK(diagnostics[0].element_index == 0);

    CHECK(diagnostics[1].element_id == "<unknown>");
    CHECK(diagnostics[1].element_type == "unknown_type_2");
    CHECK(diagnostics[1].element_index == 2);
}

TEST_CASE("UI element schema: validate_ui_element_types assigns unknown id when missing", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["elements"] = nlohmann::json::array();

    nlohmann::json el = nlohmann::json::object();
    el["type"] = "unsupported_type";
    screen["elements"].push_back(el);

    auto diagnostics = validate_ui_element_types(screen);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].element_id == "<unknown>");
    CHECK(diagnostics[0].element_type == "unsupported_type");
}

TEST_CASE("UI element schema: validate_ui_element_types reports missing type", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["elements"] = nlohmann::json::array();
    screen["elements"].push_back({{"id", "missing_type"}});

    auto diagnostics = validate_ui_element_types(screen);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].element_id == "missing_type");
    CHECK(diagnostics[0].element_type == "<missing>");
    CHECK(diagnostics[0].element_index == 0);
}

TEST_CASE("UI element schema: validate_ui_element_types reports non-string type", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["elements"] = nlohmann::json::array();
    screen["elements"].push_back({{"id", "numeric_type"}, {"type", 42}});

    auto diagnostics = validate_ui_element_types(screen);
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].element_id == "numeric_type");
    CHECK(diagnostics[0].element_type == "<non-string:number>");
    CHECK(diagnostics[0].element_index == 0);
}

TEST_CASE("UI element schema: song_complete.json no longer has unsupported types", "[ui]") {
    std::ifstream file("content/ui/screens/song_complete.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: title.json has no unsupported types", "[ui]") {
    std::ifstream file("content/ui/screens/title.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: title settings button is renderable and ASCII", "[ui]") {
    std::ifstream file("content/ui/screens/title.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    const nlohmann::json* settings_button = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "settings_button") {
            settings_button = &el;
            break;
        }
    }

    REQUIRE(settings_button != nullptr);
    CHECK(settings_button->value("type", "") == "button");
    CHECK(settings_button->value("text", "") == "SET");
    CHECK(settings_button->value("action", "") == "settings");
    CHECK(settings_button->contains("w_n"));
    CHECK(settings_button->contains("h_n"));
    CHECK(settings_button->contains("bg_color"));
    CHECK(settings_button->contains("border_color"));
    CHECK(settings_button->contains("text_color"));
    CHECK(settings_button->at("x_n").get<float>() + settings_button->at("w_n").get<float>() <= 1.0f);
    CHECK(settings_button->at("y_n").get<float>() + settings_button->at("h_n").get<float>() <= 1.0f);
}

TEST_CASE("UI element schema: tutorial.json has renderable start button", "[ui][ftue]") {
    std::ifstream file("content/ui/screens/tutorial.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());

    const nlohmann::json* start_button = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "continue_button") {
            start_button = &el;
            break;
        }
    }

    REQUIRE(start_button != nullptr);
    CHECK(start_button->value("type", "") == "button");
    CHECK(start_button->value("text", "") == "START");
    CHECK(start_button->value("action", "") == "confirm");
    CHECK(start_button->contains("w_n"));
    CHECK(start_button->contains("h_n"));
}

TEST_CASE("UI element schema: tutorial.json demonstrates shape matching with shapes", "[ui][ftue]") {
    // #163 — tutorial must demonstrate, not just label, the match-shape mechanic.
    std::ifstream file("content/ui/screens/tutorial.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    bool has_circle = false, has_square = false, has_triangle = false;
    for (const auto& el : screen["elements"]) {
        if (el.value("type", "") != "shape") continue;
        const auto sh = el.value("shape", "");
        if (sh == "circle")   has_circle = true;
        if (sh == "square")   has_square = true;
        if (sh == "triangle") has_triangle = true;
    }
    CHECK(has_circle);
    CHECK(has_square);
    CHECK(has_triangle);
}

TEST_CASE("UI element schema: tutorial.json provides platform-aware dodge copy", "[ui][ftue]") {
    // #164 — tutorial must not assume touch on every platform.
    std::ifstream file("content/ui/screens/tutorial.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    const nlohmann::json* desktop_dodge = nullptr;
    const nlohmann::json* web_dodge = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "tutorial_dodge_desktop") desktop_dodge = &el;
        if (el.value("id", "") == "tutorial_dodge_web")     web_dodge = &el;
    }

    REQUIRE(desktop_dodge != nullptr);
    REQUIRE(web_dodge != nullptr);
    CHECK(desktop_dodge->value("type", "") == "text");
    CHECK(desktop_dodge->value("platform_only", "") == "desktop");
    CHECK(web_dodge->value("type", "") == "text");
    CHECK(web_dodge->value("platform_only", "") == "web");

    // Desktop copy should reference keys, not touch.
    const auto desktop_text = desktop_dodge->value("text", "");
    CHECK(desktop_text.find("SWIPE") == std::string::npos);
    CHECK((desktop_text.find("ARROW") != std::string::npos
        || desktop_text.find("KEY") != std::string::npos));
}

TEST_CASE("UI element schema: tutorial.json explains energy mechanic", "[ui][ftue]") {
    // #171 — first-run / tutorial must explain energy.
    std::ifstream file("content/ui/screens/tutorial.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    bool found_energy_copy = false;
    for (const auto& el : screen["elements"]) {
        if (el.value("type", "") != "text") continue;
        const auto text = el.value("text", "");
        if (text.find("ENERGY") != std::string::npos) {
            found_energy_copy = true;
            break;
        }
    }
    CHECK(found_energy_copy);
}

TEST_CASE("UI element schema: gameplay.json has ENERGY label near energy bar", "[ui]") {
    // #171 — energy bar must have a non-color cue (label) for accessibility.
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    const nlohmann::json* energy_label = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "energy_label") {
            energy_label = &el;
            break;
        }
    }

    REQUIRE(energy_label != nullptr);
    CHECK(energy_label->value("type", "") == "text");
    CHECK(energy_label->value("text", "") == "ENERGY");
    CHECK(energy_label->contains("x_n"));
    CHECK(energy_label->contains("y_n"));
}

TEST_CASE("UI element schema: settings.json exposes calibration and accessibility controls", "[ui]") {
    std::ifstream file("content/ui/screens/settings.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());

    bool found_audio_minus = false;
    bool found_audio_plus = false;
    bool found_audio_value = false;
    bool found_haptics = false;
    bool found_haptics_value = false;
    bool found_reduce_motion = false;
    bool found_reduce_motion_value = false;
    bool found_back = false;

    for (const auto& el : screen["elements"]) {
        const auto id = el.value("id", "");
        if (id == "audio_offset_minus") {
            found_audio_minus = el.value("action", "") == "audio_offset_minus"
                && el.value("text", "") == "-";
        } else if (id == "audio_offset_plus") {
            found_audio_plus = el.value("action", "") == "audio_offset_plus";
        } else if (id == "audio_offset_display") {
            found_audio_value = el.value("source", "") == "SettingsState.audio_offset_ms"
                && el.value("format", "") == "signed_ms";
        } else if (id == "haptics_toggle") {
            found_haptics = el.value("action", "") == "haptics_toggle";
        } else if (id == "haptics_value") {
            found_haptics_value = el.value("source", "") == "SettingsState.haptics_enabled"
                && el.value("format", "") == "on_off";
        } else if (id == "reduce_motion_toggle") {
            found_reduce_motion = el.value("action", "") == "reduce_motion_toggle";
        } else if (id == "reduce_motion_value") {
            found_reduce_motion_value = el.value("source", "") == "SettingsState.reduce_motion"
                && el.value("format", "") == "on_off";
        } else if (id == "close_button") {
            found_back = el.value("action", "") == "settings_back";
        }
    }

    CHECK(found_audio_minus);
    CHECK(found_audio_plus);
    CHECK(found_audio_value);
    CHECK(found_haptics);
    CHECK(found_haptics_value);
    CHECK(found_reduce_motion);
    CHECK(found_reduce_motion_value);
    CHECK(found_back);
}

TEST_CASE("UI element schema: gameplay.json has no unsupported types", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: gameplay pause button uses renderable button fields", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    const nlohmann::json* pause_button = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "pause_button") {
            pause_button = &el;
            break;
        }
    }

    REQUIRE(pause_button != nullptr);
    CHECK(pause_button->value("type", "") == "button");
    CHECK(pause_button->contains("w_n"));
    CHECK(pause_button->contains("h_n"));
    CHECK(pause_button->contains("bg_color"));
    CHECK(pause_button->contains("border_color"));
    CHECK(pause_button->contains("text_color"));
    CHECK(pause_button->at("x_n").get<float>() + pause_button->at("w_n").get<float>() <= 1.0f);
    CHECK(pause_button->at("y_n").get<float>() + pause_button->at("h_n").get<float>() <= 1.0f);
    CHECK_FALSE(pause_button->contains("width_n"));
    CHECK_FALSE(pause_button->contains("height_n"));
    CHECK_FALSE(pause_button->contains("bg"));
    CHECK_FALSE(pause_button->contains("border"));
}

TEST_CASE("UI element schema: level_select.json has no unsupported types", "[ui]") {
    std::ifstream file("content/ui/screens/level_select.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: game_over.json has no unsupported types", "[ui]") {
    std::ifstream file("content/ui/screens/game_over.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: paused.json has no unsupported types", "[ui]") {
    std::ifstream file("content/ui/screens/paused.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    auto diagnostics = validate_ui_element_types(screen);
    CHECK(diagnostics.empty());
}

TEST_CASE("UI element schema: paused resume button uses renderable bounded button fields", "[ui]") {
    std::ifstream file("content/ui/screens/paused.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    const nlohmann::json* resume_button = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "resume_button") {
            resume_button = &el;
            break;
        }
    }

    REQUIRE(resume_button != nullptr);
    CHECK(resume_button->value("type", "") == "button");
    CHECK(resume_button->value("text", "") == "RESUME");
    CHECK(resume_button->contains("w_n"));
    CHECK(resume_button->contains("h_n"));
    CHECK(resume_button->contains("bg_color"));
    CHECK(resume_button->contains("border_color"));
    CHECK(resume_button->contains("text_color"));
    CHECK(resume_button->at("x_n").get<float>() + resume_button->at("w_n").get<float>() <= 1.0f);
    CHECK(resume_button->at("y_n").get<float>() + resume_button->at("h_n").get<float>() <= 1.0f);
    CHECK(resume_button->at("w_n").get<float>() < 1.0f);
    CHECK(resume_button->at("h_n").get<float>() < 1.0f);
}

TEST_CASE("UI element schema: paused screen exposes instructions and main menu button", "[ui]") {
    std::ifstream file("content/ui/screens/paused.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    const nlohmann::json* resume_instruction = nullptr;
    const nlohmann::json* menu_instruction = nullptr;
    const nlohmann::json* menu_button = nullptr;
    for (const auto& el : screen["elements"]) {
        const auto id = el.value("id", "");
        if (id == "resume_instruction") {
            resume_instruction = &el;
        } else if (id == "menu_instruction") {
            menu_instruction = &el;
        } else if (id == "menu_button") {
            menu_button = &el;
        }
    }

    REQUIRE(resume_instruction != nullptr);
    CHECK(resume_instruction->value("type", "") == "text");
    CHECK(resume_instruction->value("text", "").find("RESUME") != std::string::npos);

    REQUIRE(menu_instruction != nullptr);
    CHECK(menu_instruction->value("type", "") == "text");
    CHECK(menu_instruction->value("text", "").find("MAIN MENU") != std::string::npos);

    REQUIRE(menu_button != nullptr);
    CHECK(menu_button->value("type", "") == "button");
    CHECK(menu_button->value("text", "") == "MAIN MENU");
    CHECK(menu_button->value("action", "") == "main_menu");
    CHECK(menu_button->contains("w_n"));
    CHECK(menu_button->contains("h_n"));
    CHECK(menu_button->contains("bg_color"));
    CHECK(menu_button->contains("border_color"));
    CHECK(menu_button->contains("text_color"));
    CHECK(menu_button->at("x_n").get<float>() + menu_button->at("w_n").get<float>() <= 1.0f);
    CHECK(menu_button->at("y_n").get<float>() + menu_button->at("h_n").get<float>() <= 1.0f);
    CHECK(menu_button->at("w_n").get<float>() < 1.0f);
    CHECK(menu_button->at("h_n").get<float>() < 1.0f);
}

// ── #312: build_ui_element_map tests ─────────────────────────────────────────

TEST_CASE("UI element map: build_ui_element_map is empty with no elements", "[ui][312]") {
    UIState ui;
    ui.screen = nlohmann::json::object();
    build_ui_element_map(ui);
    CHECK(ui.element_map.empty());
}

TEST_CASE("UI element map: build_ui_element_map indexes elements by hashed id", "[ui][312]") {
    using namespace entt::literals;

    UIState ui;
    ui.screen = {{"elements", nlohmann::json::array({
        {{"id", "shape_buttons"}, {"type", "shape_button_row"}},
        {{"id", "lane_divider"},  {"type", "line"}},
        {{"id", "song_cards"},    {"type", "card_list"}},
    })}};

    build_ui_element_map(ui);

    REQUIRE(ui.element_map.size() == 3);

    auto it_sb = ui.element_map.find("shape_buttons"_hs);
    REQUIRE(it_sb != ui.element_map.end());
    CHECK(it_sb->second == 0u);

    auto it_ld = ui.element_map.find("lane_divider"_hs);
    REQUIRE(it_ld != ui.element_map.end());
    CHECK(it_ld->second == 1u);

    auto it_sc = ui.element_map.find("song_cards"_hs);
    REQUIRE(it_sc != ui.element_map.end());
    CHECK(it_sc->second == 2u);
}

TEST_CASE("UI element map: elements without id are not indexed", "[ui][312]") {
    UIState ui;
    ui.screen = {{"elements", nlohmann::json::array({
        {{"type", "text"}},               // no id
        {{"id", "pause_button"}, {"type", "button"}},
    })}};

    build_ui_element_map(ui);

    CHECK(ui.element_map.size() == 1u);
    using namespace entt::literals;
    CHECK(ui.element_map.count("pause_button"_hs) == 1u);
}

TEST_CASE("UI element map: map is cleared and rebuilt on re-call", "[ui][312]") {
    using namespace entt::literals;

    UIState ui;
    ui.screen = {{"elements", nlohmann::json::array({
        {{"id", "title_text"}, {"type", "text"}},
    })}};
    build_ui_element_map(ui);
    REQUIRE(ui.element_map.size() == 1u);

    // Simulate screen change: replace JSON and rebuild.
    ui.screen = {{"elements", nlohmann::json::array({
        {{"id", "pause_button"}, {"type", "button"}},
        {{"id", "energy_label"}, {"type", "text"}},
    })}};
    build_ui_element_map(ui);

    CHECK(ui.element_map.size() == 2u);
    CHECK(ui.element_map.count("title_text"_hs) == 0u);
    CHECK(ui.element_map.count("pause_button"_hs) == 1u);
    CHECK(ui.element_map.count("energy_label"_hs) == 1u);
}

TEST_CASE("UI element map: gameplay.json builds non-empty map with known ids", "[ui][312]") {
    using namespace entt::literals;

    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());

    UIState ui;
    ui.screen = nlohmann::json::parse(file);
    build_ui_element_map(ui);

    CHECK_FALSE(ui.element_map.empty());
    // These ids are accessed per-frame by draw_hud.
    CHECK(ui.element_map.count("shape_buttons"_hs) == 1u);
    CHECK(ui.element_map.count("lane_divider"_hs) == 1u);
}
