#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "systems/ui_loader.h"
#include "components/ui_state.h"
#include "components/ui_layout_cache.h"
#include <fstream>

TEST_CASE("overlay: validate_overlay_schema accepts valid nested color", "[ui]") {
    nlohmann::json overlay = nlohmann::json::object();
    overlay["color"] = {0, 0, 0, 160};
    CHECK(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: validate_overlay_schema accepts RGB (no alpha)", "[ui]") {
    nlohmann::json overlay = nlohmann::json::object();
    overlay["color"] = {100, 150, 200};
    CHECK(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: validate_overlay_schema rejects missing color field", "[ui]") {
    nlohmann::json overlay = nlohmann::json::object();
    overlay["something_else"] = {0, 0, 0, 160};
    CHECK_FALSE(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: validate_overlay_schema rejects non-object", "[ui]") {
    nlohmann::json overlay = nlohmann::json::array();
    overlay.push_back(0);
    CHECK_FALSE(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: validate_overlay_schema rejects non-array color", "[ui]") {
    nlohmann::json overlay = nlohmann::json::object();
    overlay["color"] = 160;
    CHECK_FALSE(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: validate_overlay_schema rejects too few color elements", "[ui]") {
    nlohmann::json overlay = nlohmann::json::object();
    overlay["color"] = {0, 0};
    CHECK_FALSE(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: validate_overlay_schema rejects too many color elements", "[ui]") {
    nlohmann::json overlay = nlohmann::json::object();
    overlay["color"] = {0, 0, 0, 160, 50};
    CHECK_FALSE(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: validate_overlay_schema rejects non-integer color values", "[ui]") {
    nlohmann::json overlay = nlohmann::json::object();
    overlay["color"] = {0.5, 100, 200, 160};
    CHECK_FALSE(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: validate_overlay_schema rejects out-of-range color values", "[ui]") {
    nlohmann::json overlay = nlohmann::json::object();
    overlay["color"] = {0, 0, 0, 300};
    CHECK_FALSE(validate_overlay_schema(overlay));
}

TEST_CASE("overlay: extract_overlay_color from screen with valid nested overlay", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["screen"] = "paused";
    screen["overlay"] = nlohmann::json::object();
    screen["overlay"]["color"] = {0, 0, 0, 160};

    auto color = extract_overlay_color(screen);
    REQUIRE(color.has_value());
    CHECK(color.value().r == 0);
    CHECK(color.value().g == 0);
    CHECK(color.value().b == 0);
    CHECK(color.value().a == 160);
}

TEST_CASE("overlay: extract_overlay_color defaults alpha to 255", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["overlay"] = nlohmann::json::object();
    screen["overlay"]["color"] = {100, 150, 200};

    auto color = extract_overlay_color(screen);
    REQUIRE(color.has_value());
    CHECK(color.value().r == 100);
    CHECK(color.value().g == 150);
    CHECK(color.value().b == 200);
    CHECK(color.value().a == 255);
}

TEST_CASE("overlay: extract_overlay_color returns nullopt when no overlay", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["screen"] = "paused";

    auto color = extract_overlay_color(screen);
    CHECK_FALSE(color.has_value());
}

TEST_CASE("overlay: extract_overlay_color returns nullopt for invalid schema", "[ui]") {
    nlohmann::json screen = nlohmann::json::object();
    screen["overlay"] = nlohmann::json::object();
    screen["overlay"]["color"] = {0, 0};

    auto color = extract_overlay_color(screen);
    CHECK_FALSE(color.has_value());
}

TEST_CASE("overlay: extract_overlay_color returns nullopt for non-object screen", "[ui]") {
    nlohmann::json screen = nlohmann::json::array();
    screen.push_back("paused");

    auto color = extract_overlay_color(screen);
    CHECK_FALSE(color.has_value());
}

TEST_CASE("overlay: shipped overlay screens use nested overlay color schema", "[ui]") {
    const char* paths[] = {
        "content/ui/screens/paused.json",
        "content/ui/screens/game_over.json",
        "content/ui/screens/song_complete.json"
    };

    for (const char* path : paths) {
        std::ifstream file(path);
        REQUIRE(file.is_open());
        auto screen = nlohmann::json::parse(file);

        REQUIRE(screen.contains("overlay"));
        auto color = extract_overlay_color(screen);
        CAPTURE(path);
        CHECK(color.has_value());
    }
}

TEST_CASE("overlay: paused screen dim color matches content schema", "[ui]") {
    std::ifstream file("content/ui/screens/paused.json");
    REQUIRE(file.is_open());
    auto paused = nlohmann::json::parse(file);

    auto color = extract_overlay_color(paused);
    REQUIRE(color.has_value());
    CHECK(color->r == 0);
    CHECK(color->g == 0);
    CHECK(color->b == 0);
    CHECK(color->a == 160);
}

// ── build_overlay_layout cache tests (#143) ───────────────────────────────────

TEST_CASE("build_overlay_layout: empty UIState yields invalid layout", "[ui][overlay_cache]") {
    UIState ui;
    auto layout = build_overlay_layout(ui);
    CHECK_FALSE(layout.valid);
}

TEST_CASE("build_overlay_layout: missing overlay in overlay_screen yields invalid", "[ui][overlay_cache]") {
    UIState ui;
    ui.overlay_screen = nlohmann::json::object();
    ui.overlay_screen["screen"] = "paused";
    // no "overlay" key
    auto layout = build_overlay_layout(ui);
    CHECK_FALSE(layout.valid);
}

TEST_CASE("build_overlay_layout: invalid overlay schema yields invalid", "[ui][overlay_cache]") {
    UIState ui;
    ui.overlay_screen = nlohmann::json::object();
    // wrong key ("overlay_color" instead of nested "overlay.color")
    ui.overlay_screen["overlay_color"] = {0, 0, 0, 160};
    auto layout = build_overlay_layout(ui);
    CHECK_FALSE(layout.valid);
}

TEST_CASE("build_overlay_layout: valid overlay_screen yields correct color", "[ui][overlay_cache]") {
    UIState ui;
    ui.overlay_screen = nlohmann::json::object();
    ui.overlay_screen["overlay"] = nlohmann::json::object();
    ui.overlay_screen["overlay"]["color"] = {0, 0, 0, 160};

    auto layout = build_overlay_layout(ui);
    REQUIRE(layout.valid);
    CHECK(layout.dim_color.r == 0);
    CHECK(layout.dim_color.g == 0);
    CHECK(layout.dim_color.b == 0);
    CHECK(layout.dim_color.a == 160);
}

TEST_CASE("build_overlay_layout: RGB (no alpha) defaults to 255", "[ui][overlay_cache]") {
    UIState ui;
    ui.overlay_screen = nlohmann::json::object();
    ui.overlay_screen["overlay"] = nlohmann::json::object();
    ui.overlay_screen["overlay"]["color"] = {50, 100, 200};

    auto layout = build_overlay_layout(ui);
    REQUIRE(layout.valid);
    CHECK(layout.dim_color.r == 50);
    CHECK(layout.dim_color.g == 100);
    CHECK(layout.dim_color.b == 200);
    CHECK(layout.dim_color.a == 255);
}

TEST_CASE("build_overlay_layout: shipped paused.json produces correct dim color", "[ui][overlay_cache][integration]") {
    std::ifstream f("content/ui/screens/paused.json");
    REQUIRE(f.is_open());
    auto screen = nlohmann::json::parse(f);

    UIState ui;
    ui.overlay_screen = screen;
    auto layout = build_overlay_layout(ui);

    REQUIRE(layout.valid);
    CHECK(layout.dim_color.r == 0);
    CHECK(layout.dim_color.g == 0);
    CHECK(layout.dim_color.b == 0);
    CHECK(layout.dim_color.a == 160);
}
