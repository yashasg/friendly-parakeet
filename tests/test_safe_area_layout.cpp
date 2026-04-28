#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../app/constants.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Safe Area Layout Tests
// Verify that HUD elements respect mobile safe areas (notches, home indicators).
// Tests validate both virtual-space positions and letterboxed mobile windows.
// ─────────────────────────────────────────────────────────────────────────────

// Representative mobile aspect ratios (width x height):
// iPhone 14/15: 390x844 (notch at top, rounded corners)
// Pixel 6/7: 393x852 (hole punch at top, rounded corners)
// Samsung Galaxy S23: 430x932 (rounded corners)
// These determine letterbox dimensions when window scales 720x1280 to fit.

// Calculate letterbox offsets and scale when rendering virtual 720x1280 to a mobile window.
static void compute_letterbox(int window_w, int window_h,
                             float& out_scale, float& out_offset_x, float& out_offset_y) {
    // Virtual is 720x1280 (width x height).
    // Window is typically narrower (e.g., 390x844 for iPhone) and might fit letterbox.
    // Scale to fit: pick minimum of (window_w / 720, window_h / 1280) to maintain aspect.
    float scale_w = static_cast<float>(window_w) / 720.0f;
    float scale_h = static_cast<float>(window_h) / 1280.0f;
    out_scale = (scale_w < scale_h) ? scale_w : scale_h;

    // Center letterbox in window
    float virtual_w_scaled = 720.0f * out_scale;
    float virtual_h_scaled = 1280.0f * out_scale;
    out_offset_x = (window_w - virtual_w_scaled) / 2.0f;
    out_offset_y = (window_h - virtual_h_scaled) / 2.0f;
}

TEST_CASE("safe_area: constants defined", "[ui]") {
    CHECK(constants::SAFE_AREA_TOP_PX > 0.0f);
    CHECK(constants::SAFE_AREA_BOTTOM_PX > 0.0f);
    CHECK(constants::SAFE_AREA_LEFT_PX > 0.0f);
    CHECK(constants::SAFE_AREA_RIGHT_PX > 0.0f);
    CHECK(constants::SAFE_AREA_TOP_N > 0.0f);
    CHECK(constants::SAFE_AREA_BOTTOM_N > 0.0f);
    CHECK(constants::SAFE_AREA_LEFT_N > 0.0f);
    CHECK(constants::SAFE_AREA_RIGHT_N > 0.0f);
}

TEST_CASE("safe_area: normalized safe area constants in [0, 1]", "[ui]") {
    CHECK(constants::SAFE_AREA_TOP_N >= 0.0f);
    CHECK(constants::SAFE_AREA_TOP_N <= 1.0f);
    CHECK(constants::SAFE_AREA_BOTTOM_N >= 0.0f);
    CHECK(constants::SAFE_AREA_BOTTOM_N <= 1.0f);
    CHECK(constants::SAFE_AREA_LEFT_N >= 0.0f);
    CHECK(constants::SAFE_AREA_LEFT_N <= 1.0f);
    CHECK(constants::SAFE_AREA_RIGHT_N >= 0.0f);
    CHECK(constants::SAFE_AREA_RIGHT_N <= 1.0f);
}

TEST_CASE("safe_area: pixel/normalized safe area consistency", "[ui]") {
    // Normalized values should match pixel values scaled by screen dimensions
    float computed_top_n = constants::SAFE_AREA_TOP_PX / constants::SCREEN_H;
    float computed_bottom_n = constants::SAFE_AREA_BOTTOM_PX / constants::SCREEN_H;
    float computed_left_n = constants::SAFE_AREA_LEFT_PX / constants::SCREEN_W;
    float computed_right_n = constants::SAFE_AREA_RIGHT_PX / constants::SCREEN_W;

    CHECK_THAT(constants::SAFE_AREA_TOP_N,
               Catch::Matchers::WithinAbs(computed_top_n, 0.001f));
    CHECK_THAT(constants::SAFE_AREA_BOTTOM_N,
               Catch::Matchers::WithinAbs(computed_bottom_n, 0.001f));
    CHECK_THAT(constants::SAFE_AREA_LEFT_N,
               Catch::Matchers::WithinAbs(computed_left_n, 0.001f));
    CHECK_THAT(constants::SAFE_AREA_RIGHT_N,
               Catch::Matchers::WithinAbs(computed_right_n, 0.001f));
}

TEST_CASE("safe_area: energy bar inside left safe padding", "[ui]") {
    // Energy bar x position should be at or beyond left safe area padding
    CHECK(constants::ENERGY_BAR_X >= constants::SAFE_AREA_LEFT_PX);
}

TEST_CASE("safe_area: score and high_score above top safe padding", "[ui]") {
    // Score should start below top safe area margin
    float score_y_px = constants::HUD_SCORE_Y_N * constants::SCREEN_H;
    CHECK(score_y_px >= constants::SAFE_AREA_TOP_PX);

    float hiscore_y_px = constants::HUD_HISCORE_Y_N * constants::SCREEN_H;
    CHECK(hiscore_y_px >= constants::SAFE_AREA_TOP_PX);
}

TEST_CASE("safe_area: button row above bottom safe padding", "[ui]") {
    // Shape buttons should not extend below safe bottom area
    float button_y_px = constants::BUTTON_Y_N * constants::SCREEN_H;
    float button_h_px = constants::BUTTON_H_N * constants::SCREEN_H;
    float button_bottom = button_y_px + button_h_px;
    float safe_bottom_edge = constants::SCREEN_H - constants::SAFE_AREA_BOTTOM_PX;

    CHECK(button_bottom <= safe_bottom_edge);
}

TEST_CASE("safe_area: gameplay.json score respects top safe area", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = json::parse(file);

    const json* score_el = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "score") {
            score_el = &el;
            break;
        }
    }

    REQUIRE(score_el != nullptr);
    float score_y_n = score_el->at("y_n").get<float>();
    CHECK(score_y_n >= constants::SAFE_AREA_TOP_N);
}

TEST_CASE("safe_area: gameplay.json high_score respects top safe area", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = json::parse(file);

    const json* hi_el = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "high_score") {
            hi_el = &el;
            break;
        }
    }

    REQUIRE(hi_el != nullptr);
    float hiscore_y_n = hi_el->at("y_n").get<float>();
    CHECK(hiscore_y_n >= constants::SAFE_AREA_TOP_N);
}

TEST_CASE("safe_area: gameplay.json pause button top respects safe area", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = json::parse(file);

    const json* pause_el = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "pause_button") {
            pause_el = &el;
            break;
        }
    }

    REQUIRE(pause_el != nullptr);
    float pause_y_n = pause_el->at("y_n").get<float>();
    CHECK(pause_y_n >= constants::SAFE_AREA_TOP_N);
}

TEST_CASE("safe_area: gameplay.json pause button right respects safe area", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = json::parse(file);

    const json* pause_el = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "pause_button") {
            pause_el = &el;
            break;
        }
    }

    REQUIRE(pause_el != nullptr);
    float pause_x_n = pause_el->at("x_n").get<float>();
    float pause_w_n = pause_el->at("w_n").get<float>();
    float pause_right_n = pause_x_n + pause_w_n;
    CHECK(pause_right_n <= (1.0f - constants::SAFE_AREA_RIGHT_N));
}

TEST_CASE("safe_area: gameplay.json shape buttons above bottom safe area", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = json::parse(file);

    const json* sb = nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == "shape_buttons") {
            sb = &el;
            break;
        }
    }

    REQUIRE(sb != nullptr);
    float button_y_n = sb->at("y_n").get<float>();
    float button_h_n = sb->at("button_h_n").get<float>();
    float button_bottom_n = button_y_n + button_h_n;

    CHECK(button_bottom_n <= (1.0f - constants::SAFE_AREA_BOTTOM_N));
}

TEST_CASE("safe_area: mobile letterbox iPhone 14/15 score visible", "[ui]") {
    // iPhone 14/15: 390x844
    float scale, offset_x, offset_y;
    compute_letterbox(390, 844, scale, offset_x, offset_y);

    // Virtual score position
    float score_y_virt = constants::HUD_SCORE_Y_N * constants::SCREEN_H;
    float score_x_virt = constants::HUD_SCORE_X_N * constants::SCREEN_W;

    // Screen position after letterbox
    float score_y_screen = offset_y + score_y_virt * scale;
    float score_x_screen = offset_x + score_x_virt * scale;

    // Should be within viewport and above notch (~40-50px top padding on iPhone)
    CHECK(score_y_screen >= 30.0f);
    CHECK(score_x_screen >= 0.0f);
    CHECK(score_x_screen <= 390.0f);
}

TEST_CASE("safe_area: mobile letterbox iPhone 14/15 buttons visible", "[ui]") {
    // iPhone 14/15: 390x844
    float scale, offset_x, offset_y;
    compute_letterbox(390, 844, scale, offset_x, offset_y);

    // Virtual button position
    float button_y_virt = constants::BUTTON_Y_N * constants::SCREEN_H;
    float button_h_virt = constants::BUTTON_H_N * constants::SCREEN_H;

    // Screen position after letterbox
    float button_bottom_screen = offset_y + (button_y_virt + button_h_virt) * scale;

    // Should be within viewport and above home indicator (typically 50-60px from bottom)
    CHECK(button_bottom_screen <= (844.0f - 40.0f));
}

TEST_CASE("safe_area: mobile letterbox Pixel 6/7 score visible", "[ui]") {
    // Pixel 6/7: 393x852
    float scale, offset_x, offset_y;
    compute_letterbox(393, 852, scale, offset_x, offset_y);

    float score_y_virt = constants::HUD_SCORE_Y_N * constants::SCREEN_H;
    float score_y_screen = offset_y + score_y_virt * scale;

    // Should be within viewport and below hole punch (~50px)
    CHECK(score_y_screen >= 30.0f);
}

TEST_CASE("safe_area: mobile letterbox Pixel 6/7 buttons visible", "[ui]") {
    // Pixel 6/7: 393x852
    float scale, offset_x, offset_y;
    compute_letterbox(393, 852, scale, offset_x, offset_y);

    float button_y_virt = constants::BUTTON_Y_N * constants::SCREEN_H;
    float button_h_virt = constants::BUTTON_H_N * constants::SCREEN_H;
    float button_bottom_screen = offset_y + (button_y_virt + button_h_virt) * scale;

    // Should be within viewport and above navigation bar (~48px)
    CHECK(button_bottom_screen <= (852.0f - 40.0f));
}

TEST_CASE("safe_area: mobile letterbox Galaxy S23 score visible", "[ui]") {
    // Samsung Galaxy S23: 430x932
    float scale, offset_x, offset_y;
    compute_letterbox(430, 932, scale, offset_x, offset_y);

    float score_y_virt = constants::HUD_SCORE_Y_N * constants::SCREEN_H;
    float score_y_screen = offset_y + score_y_virt * scale;

    // Should be within viewport and respect rounded corners
    CHECK(score_y_screen >= 20.0f);
}

TEST_CASE("safe_area: mobile letterbox Galaxy S23 buttons visible", "[ui]") {
    // Samsung Galaxy S23: 430x932
    float scale, offset_x, offset_y;
    compute_letterbox(430, 932, scale, offset_x, offset_y);

    float button_y_virt = constants::BUTTON_Y_N * constants::SCREEN_H;
    float button_h_virt = constants::BUTTON_H_N * constants::SCREEN_H;
    float button_bottom_screen = offset_y + (button_y_virt + button_h_virt) * scale;

    // Should be within viewport
    CHECK(button_bottom_screen <= 932.0f - 40.0f);
}

TEST_CASE("safe_area: score before high_score vertically", "[ui]") {
    float score_y = constants::HUD_SCORE_Y_N * constants::SCREEN_H;
    float hiscore_y = constants::HUD_HISCORE_Y_N * constants::SCREEN_H;
    CHECK(score_y < hiscore_y);
}

TEST_CASE("safe_area: gameplay.json energy_label respects left safe area", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = json::parse(file);

    const json* el = nullptr;
    for (const auto& e : screen["elements"]) {
        if (e.value("id", "") == "energy_label") { el = &e; break; }
    }
    REQUIRE(el != nullptr);
    float x_n = el->at("x_n").get<float>();
    float y_n = el->at("y_n").get<float>();
    CHECK(x_n >= constants::SAFE_AREA_LEFT_N);
    CHECK(y_n <= (1.0f - constants::SAFE_AREA_BOTTOM_N));
}

TEST_CASE("safe_area: button row and lane divider alignment", "[ui]") {
    std::ifstream file("content/ui/screens/gameplay.json");
    REQUIRE(file.is_open());
    auto screen = json::parse(file);

    float button_y_n = 0.0f;
    float divider_y_n = 0.0f;

    for (const auto& el : screen["elements"]) {
        const auto id = el.value("id", "");
        if (id == "shape_buttons") {
            button_y_n = el.at("y_n").get<float>();
        } else if (id == "lane_divider") {
            divider_y_n = el.at("y_n").get<float>();
        }
    }

    // Divider should be above buttons
    CHECK(divider_y_n < button_y_n);
}
