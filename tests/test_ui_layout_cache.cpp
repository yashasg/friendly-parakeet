// Tests for #322: UI layout cache construction.
// Verifies that build_hud_layout() and build_level_select_layout() correctly
// extract and pre-multiply layout data from JSON, and return valid=false when
// required elements are absent.

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "systems/ui_loader.h"
#include "components/ui_state.h"
#include "components/ui_layout_cache.h"
#include "constants.h"
#include <fstream>
#include <cmath>

using json = nlohmann::json;

// ── Helpers ──────────────────────────────────────────────────────────────────

static UIState make_ui_with_screen(const json& screen) {
    UIState ui;
    ui.screen = screen;
    build_ui_element_map(ui);
    return ui;
}

static bool approx(float a, float b) {
    return std::fabs(a - b) < 0.5f;  // half-pixel tolerance
}

// ── HudLayout: invalid cases ─────────────────────────────────────────────────

TEST_CASE("build_hud_layout: empty UIState yields invalid layout", "[ui][layout_cache]") {
    UIState ui;
    auto layout = build_hud_layout(ui);
    CHECK_FALSE(layout.valid);
}

TEST_CASE("build_hud_layout: missing shape_buttons element yields invalid", "[ui][layout_cache]") {
    json screen;
    screen["elements"] = json::array();
    json lane;
    lane["id"] = "lane_divider";
    lane["type"] = "line";
    lane["y_n"] = 0.875f;
    lane["color"] = {40, 40, 60, 200};
    screen["elements"].push_back(lane);

    auto ui = make_ui_with_screen(screen);
    auto layout = build_hud_layout(ui);
    CHECK_FALSE(layout.valid);
}

TEST_CASE("build_hud_layout: missing approach_ring field yields invalid", "[ui][layout_cache]") {
    json screen;
    screen["elements"] = json::array();
    json sb;
    sb["id"] = "shape_buttons";
    sb["type"] = "shape_button_row";
    sb["y_n"] = 0.891f;
    sb["button_w_n"] = 0.194f;
    sb["button_h_n"] = 0.078f;
    sb["spacing_n"] = 0.083f;
    sb["active_bg"] = {60, 60, 100, 255};
    sb["inactive_bg"] = {30, 30, 50, 200};
    sb["active_border"] = {120, 180, 255, 255};
    sb["inactive_border"] = {60, 60, 80, 255};
    sb["active_icon"] = {200, 230, 255, 255};
    sb["inactive_icon"] = {100, 100, 120, 200};
    // approach_ring intentionally omitted
    screen["elements"].push_back(sb);

    auto ui = make_ui_with_screen(screen);
    auto layout = build_hud_layout(ui);
    CHECK_FALSE(layout.valid);
}

// ── HudLayout: valid construction from inline JSON ───────────────────────────

TEST_CASE("build_hud_layout: valid JSON builds correct layout", "[ui][layout_cache]") {
    json screen;
    screen["elements"] = json::array();

    json sb;
    sb["id"] = "shape_buttons";
    sb["type"] = "shape_button_row";
    sb["y_n"] = 0.891f;
    sb["button_w_n"] = 0.194f;
    sb["button_h_n"] = 0.078f;
    sb["spacing_n"] = 0.083f;
    sb["active_bg"] = {60, 60, 100, 255};
    sb["inactive_bg"] = {30, 30, 50, 200};
    sb["active_border"] = {120, 180, 255, 255};
    sb["inactive_border"] = {60, 60, 80, 255};
    sb["active_icon"] = {200, 230, 255, 255};
    sb["inactive_icon"] = {100, 100, 120, 200};
    sb["approach_ring"]["max_radius_scale"] = 2.0f;
    sb["approach_ring"]["perfect_color"] = {100, 255, 100};
    sb["approach_ring"]["near_color"] = {180, 255, 100};
    sb["approach_ring"]["far_color"] = {120, 120, 180};
    screen["elements"].push_back(sb);

    json ld;
    ld["id"] = "lane_divider";
    ld["type"] = "line";
    ld["y_n"] = 0.875f;
    ld["color"] = {40, 40, 60, 200};
    screen["elements"].push_back(ld);

    auto ui = make_ui_with_screen(screen);
    auto layout = build_hud_layout(ui);

    REQUIRE(layout.valid);

    CHECK(approx(layout.btn_w,       0.194f * constants::SCREEN_W_F));
    CHECK(approx(layout.btn_h,       0.078f * constants::SCREEN_H_F));
    CHECK(approx(layout.btn_spacing, 0.083f * constants::SCREEN_W_F));
    CHECK(approx(layout.btn_y,       0.891f * constants::SCREEN_H_F));

    CHECK(layout.active_bg.r == 60);
    CHECK(layout.active_bg.g == 60);
    CHECK(layout.active_bg.b == 100);
    CHECK(layout.active_bg.a == 255);

    CHECK(layout.inactive_icon.r == 100);
    CHECK(layout.inactive_icon.a == 200);

    CHECK(approx(layout.approach_ring_max_radius_scale, 2.0f));
    CHECK(layout.ring_perfect.g == 255);
    CHECK(layout.ring_perfect.a == 255);  // default alpha for 3-component array

    CHECK(layout.has_lane_divider);
    CHECK(approx(layout.lane_divider_y, 0.875f * constants::SCREEN_H_F));
    CHECK(layout.lane_divider_color.a == 200);
}

TEST_CASE("build_hud_layout: without lane_divider sets has_lane_divider false",
          "[ui][layout_cache]") {
    json screen;
    screen["elements"] = json::array();

    json sb;
    sb["id"] = "shape_buttons";
    sb["type"] = "shape_button_row";
    sb["y_n"] = 0.891f;
    sb["button_w_n"] = 0.194f;
    sb["button_h_n"] = 0.078f;
    sb["spacing_n"] = 0.083f;
    sb["active_bg"] = {60, 60, 100, 255};
    sb["inactive_bg"] = {30, 30, 50, 200};
    sb["active_border"] = {120, 180, 255, 255};
    sb["inactive_border"] = {60, 60, 80, 255};
    sb["active_icon"] = {200, 230, 255, 255};
    sb["inactive_icon"] = {100, 100, 120, 200};
    sb["approach_ring"]["max_radius_scale"] = 2.0f;
    sb["approach_ring"]["perfect_color"] = {100, 255, 100};
    sb["approach_ring"]["near_color"] = {180, 255, 100};
    sb["approach_ring"]["far_color"] = {120, 120, 180};
    screen["elements"].push_back(sb);

    auto ui = make_ui_with_screen(screen);
    auto layout = build_hud_layout(ui);

    REQUIRE(layout.valid);
    CHECK_FALSE(layout.has_lane_divider);
}

// ── HudLayout: integration against shipped gameplay.json ─────────────────────

TEST_CASE("build_hud_layout: shipped gameplay.json produces valid layout",
          "[ui][layout_cache][integration]") {
    std::ifstream f("content/ui/screens/gameplay.json");
    REQUIRE(f.is_open());
    json screen = json::parse(f);

    UIState ui;
    ui.screen = screen;
    build_ui_element_map(ui);
    auto layout = build_hud_layout(ui);

    REQUIRE(layout.valid);
    CHECK(layout.btn_w > 0.0f);
    CHECK(layout.btn_h > 0.0f);
    CHECK(layout.btn_spacing > 0.0f);
    CHECK(layout.btn_y > 0.0f);
    CHECK(layout.approach_ring_max_radius_scale > 0.0f);
    CHECK(layout.has_lane_divider);
    CHECK(layout.lane_divider_y > 0.0f);
}

// ── LevelSelectLayout: invalid cases ─────────────────────────────────────────

TEST_CASE("build_level_select_layout: empty UIState yields invalid", "[ui][layout_cache]") {
    UIState ui;
    auto layout = build_level_select_layout(ui);
    CHECK_FALSE(layout.valid);
}

TEST_CASE("build_level_select_layout: missing song_cards yields invalid",
          "[ui][layout_cache]") {
    json screen;
    screen["elements"] = json::array();

    auto ui = make_ui_with_screen(screen);
    auto layout = build_level_select_layout(ui);
    CHECK_FALSE(layout.valid);
}

TEST_CASE("build_level_select_layout: missing difficulty_buttons yields invalid",
          "[ui][layout_cache]") {
    json screen;
    screen["elements"] = json::array();
    json sc;
    sc["id"] = "song_cards";
    sc["type"] = "card_list";
    sc["x_n"] = 0.0833f; sc["start_y_n"] = 0.1562f;
    sc["card_w_n"] = 0.8333f; sc["card_h_n"] = 0.1562f; sc["card_gap_n"] = 0.0312f;
    sc["selected_bg"] = {40, 50, 80, 255};
    sc["unselected_bg"] = {25, 25, 40, 255};
    sc["selected_border"] = {80, 180, 255, 255};
    sc["unselected_border"] = {50, 50, 70, 255};
    sc["title_offset_x_n"] = 0.0417f; sc["title_offset_y_n"] = 0.0195f;
    screen["elements"].push_back(sc);
    // difficulty_buttons intentionally omitted

    auto ui = make_ui_with_screen(screen);
    auto layout = build_level_select_layout(ui);
    CHECK_FALSE(layout.valid);
}

// ── LevelSelectLayout: valid construction from inline JSON ───────────────────

TEST_CASE("build_level_select_layout: valid JSON builds correct layout",
          "[ui][layout_cache]") {
    json screen;
    screen["elements"] = json::array();

    json sc;
    sc["id"] = "song_cards";
    sc["type"] = "card_list";
    sc["x_n"] = 0.0833f; sc["start_y_n"] = 0.1562f;
    sc["card_w_n"] = 0.8333f; sc["card_h_n"] = 0.1562f; sc["card_gap_n"] = 0.0312f;
    sc["corner_radius"] = 0.1f;
    sc["selected_bg"] = {40, 50, 80, 255};
    sc["unselected_bg"] = {25, 25, 40, 255};
    sc["selected_border"] = {80, 180, 255, 255};
    sc["unselected_border"] = {50, 50, 70, 255};
    sc["title_offset_x_n"] = 0.0417f; sc["title_offset_y_n"] = 0.0195f;
    screen["elements"].push_back(sc);

    json db;
    db["id"] = "difficulty_buttons";
    db["type"] = "button_row";
    db["y_offset_n"] = 0.0938f; db["x_start_n"] = 0.1389f;
    db["button_w_n"] = 0.2222f; db["button_h_n"] = 0.0391f; db["button_gap_n"] = 0.0278f;
    db["corner_radius"] = 0.2f;
    db["active_bg"] = {80, 180, 255, 255};
    db["active_border"] = {120, 220, 255, 255};
    db["active_text"] = {0, 0, 0, 255};
    db["inactive_bg"] = {35, 35, 55, 255};
    db["inactive_border"] = {60, 60, 80, 255};
    db["inactive_text"] = {180, 180, 180, 255};
    screen["elements"].push_back(db);

    auto ui = make_ui_with_screen(screen);
    auto layout = build_level_select_layout(ui);

    REQUIRE(layout.valid);

    CHECK(approx(layout.card_x,    0.0833f * constants::SCREEN_W_F));
    CHECK(approx(layout.start_y,   0.1562f * constants::SCREEN_H_F));
    CHECK(approx(layout.card_w,    0.8333f * constants::SCREEN_W_F));
    CHECK(approx(layout.card_h,    0.1562f * constants::SCREEN_H_F));
    CHECK(approx(layout.card_gap,  0.0312f * constants::SCREEN_H_F));
    CHECK(approx(layout.card_corner_radius, 0.1f));

    CHECK(layout.selected_bg.r == 40);
    CHECK(layout.unselected_bg.r == 25);
    CHECK(layout.selected_border.g == 180);
    CHECK(layout.unselected_border.r == 50);

    CHECK(approx(layout.title_offset_x, 0.0417f * constants::SCREEN_W_F));
    CHECK(approx(layout.title_offset_y, 0.0195f * constants::SCREEN_H_F));

    CHECK(approx(layout.diff_y_offset, 0.0938f * constants::SCREEN_H_F));
    CHECK(approx(layout.dx_start,      0.1389f * constants::SCREEN_W_F));
    CHECK(approx(layout.diff_btn_w,    0.2222f * constants::SCREEN_W_F));
    CHECK(approx(layout.diff_btn_h,    0.0391f * constants::SCREEN_H_F));
    CHECK(approx(layout.diff_btn_gap,  0.0278f * constants::SCREEN_W_F));
    CHECK(approx(layout.diff_corner_radius, 0.2f));

    CHECK(layout.diff_active_bg.r == 80);
    CHECK(layout.diff_active_text.r == 0);
    CHECK(layout.diff_inactive_text.r == 180);
}

// ── LevelSelectLayout: integration against shipped level_select.json ─────────

TEST_CASE("build_level_select_layout: shipped level_select.json produces valid layout",
          "[ui][layout_cache][integration]") {
    std::ifstream f("content/ui/screens/level_select.json");
    REQUIRE(f.is_open());
    json screen = json::parse(f);

    UIState ui;
    ui.screen = screen;
    build_ui_element_map(ui);
    auto layout = build_level_select_layout(ui);

    REQUIRE(layout.valid);
    CHECK(layout.card_x > 0.0f);
    CHECK(layout.start_y > 0.0f);
    CHECK(layout.card_w > 0.0f);
    CHECK(layout.card_h > 0.0f);
    CHECK(layout.diff_btn_w > 0.0f);
    CHECK(layout.diff_btn_h > 0.0f);
}
