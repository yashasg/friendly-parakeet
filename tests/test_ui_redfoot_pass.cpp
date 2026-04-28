// Tests for the Redfoot UI/UX pass — TestFlight #163, #164, #171.
//
// These tests assert that:
//   * Tutorial demonstrates the shape-matching mechanic with concrete shapes
//     (#163: tutorial must show, not just label, mechanics).
//   * Tutorial provides platform-aware dodge copy via the `platform_only`
//     field on text elements (#164: tutorial must not assume touch on every
//     platform).
//   * Tutorial explains the energy mechanic before play (#171 onboarding).
//   * Gameplay HUD has an ENERGY label near the bar (#171 non-color cue).
//
// Tests load the shipped JSON content directly so they catch content
// regressions without depending on the runtime's UI loader internals.

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <string>

using json = nlohmann::json;

namespace {
const json* find_by_id(const json& screen, const std::string& id) {
    if (!screen.contains("elements")) return nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == id) return &el;
    }
    return nullptr;
}

json load_screen(const std::string& path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());
    return json::parse(f);
}
}

TEST_CASE("redfoot/#163: tutorial demonstrates shapes with shape elements", "[ui][ftue][redfoot]") {
    auto screen = load_screen("content/ui/screens/tutorial.json");

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

TEST_CASE("redfoot/#164: tutorial provides desktop and web dodge copy via platform_only", "[ui][ftue][redfoot]") {
    auto screen = load_screen("content/ui/screens/tutorial.json");

    const json* desktop_dodge = find_by_id(screen, "tutorial_dodge_desktop");
    const json* web_dodge     = find_by_id(screen, "tutorial_dodge_web");

    REQUIRE(desktop_dodge != nullptr);
    REQUIRE(web_dodge     != nullptr);

    CHECK(desktop_dodge->value("type", "") == "text");
    CHECK(desktop_dodge->value("platform_only", "") == "desktop");
    CHECK(web_dodge->value("type", "") == "text");
    CHECK(web_dodge->value("platform_only", "") == "web");

    // Desktop copy must reference keys, not touch gestures.
    const auto desktop_text = desktop_dodge->value("text", "");
    CHECK(desktop_text.find("SWIPE") == std::string::npos);
    CHECK((desktop_text.find("ARROW") != std::string::npos
        || desktop_text.find("KEY") != std::string::npos));

    // Web copy may reference touch gestures (touch-friendly).
    const auto web_text = web_dodge->value("text", "");
    CHECK(web_text.find("SWIPE") != std::string::npos);
}

TEST_CASE("redfoot/#171: tutorial explains energy mechanic", "[ui][ftue][redfoot]") {
    auto screen = load_screen("content/ui/screens/tutorial.json");

    bool found = false;
    for (const auto& el : screen["elements"]) {
        if (el.value("type", "") != "text") continue;
        if (el.value("text", "").find("ENERGY") != std::string::npos) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("redfoot/#171: gameplay HUD has ENERGY label near the bar", "[ui][redfoot]") {
    auto screen = load_screen("content/ui/screens/gameplay.json");

    const json* label = find_by_id(screen, "energy_label");
    REQUIRE(label != nullptr);
    CHECK(label->value("type", "") == "text");
    CHECK(label->value("text", "") == "ENERGY");
    CHECK(label->contains("x_n"));
    CHECK(label->contains("y_n"));

    // Label should sit above the energy bar (BAR_TOP=770/1280 ≈ 0.602)
    // and stay within the viewport.
    const float y_n = label->at("y_n").get<float>();
    CHECK(y_n > 0.0f);
    CHECK(y_n < 0.602f);
}

TEST_CASE("redfoot/#163-#164: tutorial title and start button preserved", "[ui][ftue][redfoot]") {
    auto screen = load_screen("content/ui/screens/tutorial.json");

    const json* title = find_by_id(screen, "tutorial_title");
    const json* start = find_by_id(screen, "continue_button");
    REQUIRE(title != nullptr);
    REQUIRE(start != nullptr);
    CHECK(start->value("type", "") == "button");
    CHECK(start->value("action", "") == "confirm");
    CHECK(start->at("y_n").get<float>() + start->at("h_n").get<float>() <= 1.0f);
}
