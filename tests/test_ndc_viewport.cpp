#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ─────────────────────────────────────────────────────────────────────────────
// NDC Viewport Constants
// All *_N constants must lie in [0, 1] and must round-trip to the intended
// virtual-pixel value when multiplied by SCREEN_W or SCREEN_H.
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("ndc: VIEWPORT_CX_N is 0.5", "[ndc]") {
    CHECK(constants::VIEWPORT_CX_N == 0.5f);
}

TEST_CASE("ndc: all NDC constants are in range [0, 1]", "[ndc]") {
    CHECK(constants::VIEWPORT_CX_N               >= 0.0f); CHECK(constants::VIEWPORT_CX_N               <= 1.0f);
    CHECK(constants::HUD_SCORE_X_N               >= 0.0f); CHECK(constants::HUD_SCORE_X_N               <= 1.0f);
    CHECK(constants::HUD_SCORE_Y_N               >= 0.0f); CHECK(constants::HUD_SCORE_Y_N               <= 1.0f);
    CHECK(constants::HUD_HISCORE_Y_N             >= 0.0f); CHECK(constants::HUD_HISCORE_Y_N             <= 1.0f);
    CHECK(constants::BUTTON_Y_N                  >= 0.0f); CHECK(constants::BUTTON_Y_N                  <= 1.0f);
    CHECK(constants::BUTTON_W_N                  >= 0.0f); CHECK(constants::BUTTON_W_N                  <= 1.0f);
    CHECK(constants::BUTTON_H_N                  >= 0.0f); CHECK(constants::BUTTON_H_N                  <= 1.0f);
    CHECK(constants::BUTTON_SPACING_N            >= 0.0f); CHECK(constants::BUTTON_SPACING_N            <= 1.0f);
    CHECK(constants::BURNOUT_BAR_Y_N             >= 0.0f); CHECK(constants::BURNOUT_BAR_Y_N             <= 1.0f);
    CHECK(constants::BURNOUT_BAR_H_N             >= 0.0f); CHECK(constants::BURNOUT_BAR_H_N             <= 1.0f);
    CHECK(constants::SCENE_TITLE_SHAPES_Y_N      >= 0.0f); CHECK(constants::SCENE_TITLE_SHAPES_Y_N      <= 1.0f);
    CHECK(constants::SCENE_TITLE_SHAPES_OFFSET_N >= 0.0f); CHECK(constants::SCENE_TITLE_SHAPES_OFFSET_N <= 1.0f);
    CHECK(constants::SCENE_TITLE_SHAPES_SIZE_N   >= 0.0f); CHECK(constants::SCENE_TITLE_SHAPES_SIZE_N   <= 1.0f);
    CHECK(constants::SCENE_TITLE_TEXT_Y_N        >= 0.0f); CHECK(constants::SCENE_TITLE_TEXT_Y_N        <= 1.0f);
    CHECK(constants::SCENE_TITLE_PROMPT_Y_N      >= 0.0f); CHECK(constants::SCENE_TITLE_PROMPT_Y_N      <= 1.0f);
    CHECK(constants::SCENE_GO_TITLE_Y_N          >= 0.0f); CHECK(constants::SCENE_GO_TITLE_Y_N          <= 1.0f);
    CHECK(constants::SCENE_GO_SCORE_Y_N          >= 0.0f); CHECK(constants::SCENE_GO_SCORE_Y_N          <= 1.0f);
    CHECK(constants::SCENE_GO_HISCORE_Y_N        >= 0.0f); CHECK(constants::SCENE_GO_HISCORE_Y_N        <= 1.0f);
    CHECK(constants::SCENE_GO_PROMPT_Y_N         >= 0.0f); CHECK(constants::SCENE_GO_PROMPT_Y_N         <= 1.0f);
    CHECK(constants::SCENE_SC_TITLE_Y_N          >= 0.0f); CHECK(constants::SCENE_SC_TITLE_Y_N          <= 1.0f);
    CHECK(constants::SCENE_SC_SLABEL_Y_N         >= 0.0f); CHECK(constants::SCENE_SC_SLABEL_Y_N         <= 1.0f);
    CHECK(constants::SCENE_SC_SCORE_Y_N          >= 0.0f); CHECK(constants::SCENE_SC_SCORE_Y_N          <= 1.0f);
    CHECK(constants::SCENE_SC_HSLABEL_Y_N        >= 0.0f); CHECK(constants::SCENE_SC_HSLABEL_Y_N        <= 1.0f);
    CHECK(constants::SCENE_SC_HISCORE_Y_N        >= 0.0f); CHECK(constants::SCENE_SC_HISCORE_Y_N        <= 1.0f);
    CHECK(constants::SCENE_SC_TIMING_Y_N         >= 0.0f); CHECK(constants::SCENE_SC_TIMING_Y_N         <= 1.0f);
    CHECK(constants::SCENE_SC_TIMING_DY_N        >= 0.0f); CHECK(constants::SCENE_SC_TIMING_DY_N        <= 1.0f);
    CHECK(constants::SCENE_SC_STATS_LX_N         >= 0.0f); CHECK(constants::SCENE_SC_STATS_LX_N         <= 1.0f);
    CHECK(constants::SCENE_SC_STATS_RX_N         >= 0.0f); CHECK(constants::SCENE_SC_STATS_RX_N         <= 1.0f);
    CHECK(constants::SCENE_SC_PROMPT_Y_N         >= 0.0f); CHECK(constants::SCENE_SC_PROMPT_Y_N         <= 1.0f);
    CHECK(constants::SCENE_PAUSE_Y_N             >= 0.0f); CHECK(constants::SCENE_PAUSE_Y_N             <= 1.0f);
}

TEST_CASE("ndc: scene overlay positions are vertically ordered", "[ndc]") {
    // Game Over: title → score → high score → prompt
    CHECK(constants::SCENE_GO_TITLE_Y_N   < constants::SCENE_GO_SCORE_Y_N);
    CHECK(constants::SCENE_GO_SCORE_Y_N   < constants::SCENE_GO_HISCORE_Y_N);
    CHECK(constants::SCENE_GO_HISCORE_Y_N < constants::SCENE_GO_PROMPT_Y_N);

    // Song Complete: title → score label → score → hi-score label → hi-score → timing → prompt
    CHECK(constants::SCENE_SC_TITLE_Y_N   < constants::SCENE_SC_SLABEL_Y_N);
    CHECK(constants::SCENE_SC_SLABEL_Y_N  < constants::SCENE_SC_SCORE_Y_N);
    CHECK(constants::SCENE_SC_SCORE_Y_N   < constants::SCENE_SC_HSLABEL_Y_N);
    CHECK(constants::SCENE_SC_HSLABEL_Y_N < constants::SCENE_SC_HISCORE_Y_N);
    CHECK(constants::SCENE_SC_HISCORE_Y_N < constants::SCENE_SC_TIMING_Y_N);
    CHECK(constants::SCENE_SC_TIMING_Y_N  < constants::SCENE_SC_PROMPT_Y_N);

    // Title scene: shapes → title text → prompt
    CHECK(constants::SCENE_TITLE_SHAPES_Y_N < constants::SCENE_TITLE_TEXT_Y_N);
    CHECK(constants::SCENE_TITLE_TEXT_Y_N   < constants::SCENE_TITLE_PROMPT_Y_N);
}

TEST_CASE("ndc: stats columns LX is left of RX", "[ndc]") {
    CHECK(constants::SCENE_SC_STATS_LX_N < constants::SCENE_SC_STATS_RX_N);
}

TEST_CASE("ndc: HUD score row above high-score row", "[ndc]") {
    CHECK(constants::HUD_SCORE_Y_N < constants::HUD_HISCORE_Y_N);
}

TEST_CASE("ndc: button NDC constants round-trip to pixel constants", "[ndc]") {
    // The *_N constants are defined as pixel / dimension; multiplying back must
    // reproduce the pixel value (within float rounding: < 0.01 px).
    using Catch::Matchers::WithinAbs;
    CHECK_THAT(constants::BUTTON_Y_N * static_cast<float>(constants::SCREEN_H),
               WithinAbs(constants::BUTTON_Y, 0.01f));
    CHECK_THAT(constants::BUTTON_W_N * static_cast<float>(constants::SCREEN_W),
               WithinAbs(constants::BUTTON_W, 0.01f));
    CHECK_THAT(constants::BUTTON_H_N * static_cast<float>(constants::SCREEN_H),
               WithinAbs(constants::BUTTON_H, 0.01f));
    CHECK_THAT(constants::BUTTON_SPACING_N * static_cast<float>(constants::SCREEN_W),
               WithinAbs(constants::BUTTON_SPACING, 0.01f));
    CHECK_THAT(constants::BURNOUT_BAR_Y_N * static_cast<float>(constants::SCREEN_H),
               WithinAbs(constants::BURNOUT_BAR_Y, 0.01f));
    CHECK_THAT(constants::BURNOUT_BAR_H_N * static_cast<float>(constants::SCREEN_H),
               WithinAbs(constants::BURNOUT_BAR_H, 0.01f));
}

TEST_CASE("ndc: burnout bar is above the button row", "[ndc]") {
    // Burnout bar sits between the game area and buttons
    CHECK(constants::BURNOUT_BAR_Y_N < constants::BUTTON_Y_N);
}

// ─────────────────────────────────────────────────────────────────────────────
// ScreenTransform coordinate conversion math
// These mirror the to_vx / to_vy lambdas in input_system.cpp so that the
// coordinate mapping is tested independently of raylib.
// ─────────────────────────────────────────────────────────────────────────────

namespace {
// Mirror of the lambda inside input_system — pure math, no raylib.
float to_vx(const ScreenTransform& st, float wx) { return (wx - st.offset_x) / st.scale; }
float to_vy(const ScreenTransform& st, float wy) { return (wy - st.offset_y) / st.scale; }
}  // anonymous namespace

TEST_CASE("screen_transform: default is identity (no-op)", "[screen_transform]") {
    ScreenTransform st;  // offset_x=0, offset_y=0, scale=1
    CHECK(to_vx(st, 360.0f) == 360.0f);
    CHECK(to_vy(st, 640.0f) == 640.0f);
    CHECK(to_vx(st,   0.0f) ==   0.0f);
    CHECK(to_vy(st, 1280.0f) == 1280.0f);
}

TEST_CASE("screen_transform: scale=2 halves coordinates", "[screen_transform]") {
    ScreenTransform st;
    st.scale = 2.0f;
    using Catch::Matchers::WithinAbs;
    CHECK_THAT(to_vx(st, 100.0f), WithinAbs(50.0f, 1e-4f));
    CHECK_THAT(to_vy(st, 200.0f), WithinAbs(100.0f, 1e-4f));
    CHECK_THAT(to_vx(st, 720.0f), WithinAbs(360.0f, 1e-4f));
}

TEST_CASE("screen_transform: offset shifts origin before scaling", "[screen_transform]") {
    // Simulate a letterbox: virtual 720×1280 centred in a 1280×1280 window.
    // Horizontal bars, offset_x = (1280-720)/2 = 280, scale = 1 (same height).
    ScreenTransform st;
    st.offset_x = 280.0f;
    st.offset_y = 0.0f;
    st.scale    = 1.0f;
    using Catch::Matchers::WithinAbs;
    // Window pixel 280 → virtual 0
    CHECK_THAT(to_vx(st, 280.0f), WithinAbs(0.0f, 1e-4f));
    // Window pixel 280+360=640 → virtual 360 (horizontal centre)
    CHECK_THAT(to_vx(st, 640.0f), WithinAbs(360.0f, 1e-4f));
    // Window pixel 280+720=1000 → virtual 720
    CHECK_THAT(to_vx(st, 1000.0f), WithinAbs(720.0f, 1e-4f));
    // Y unaffected (no vertical offset)
    CHECK_THAT(to_vy(st, 640.0f), WithinAbs(640.0f, 1e-4f));
}

TEST_CASE("screen_transform: combined offset and scale (pillarbox + zoom)", "[screen_transform]") {
    // Virtual 720×1280, window 1440×2560 (2× scale, no letterbar).
    ScreenTransform st;
    st.offset_x = 0.0f;
    st.offset_y = 0.0f;
    st.scale    = 2.0f;
    using Catch::Matchers::WithinAbs;
    // Window pixel 0 → virtual 0
    CHECK_THAT(to_vx(st, 0.0f), WithinAbs(0.0f, 1e-4f));
    // Window pixel 720 → virtual 360
    CHECK_THAT(to_vx(st, 720.0f), WithinAbs(360.0f, 1e-4f));
    // Window pixel 2560 → virtual 1280
    CHECK_THAT(to_vy(st, 2560.0f), WithinAbs(1280.0f, 1e-4f));
}

TEST_CASE("screen_transform: full letterbox (offset + scale)", "[screen_transform]") {
    // Virtual 720×1280 shown in a 900×1280 window (scale=1, offset_x=90).
    ScreenTransform st;
    st.offset_x = 90.0f;
    st.offset_y = 0.0f;
    st.scale    = 1.0f;
    using Catch::Matchers::WithinAbs;
    CHECK_THAT(to_vx(st, 90.0f),  WithinAbs(0.0f,   1e-4f));
    CHECK_THAT(to_vx(st, 450.0f), WithinAbs(360.0f, 1e-4f));
    CHECK_THAT(to_vx(st, 810.0f), WithinAbs(720.0f, 1e-4f));
}

// ─────────────────────────────────────────────────────────────────────────────
// Gesture system — NDC-derived button positions
// Verify that the button centres computed from NDC constants (as gesture_system
// does) are identical to those computed from the raw pixel constants, and that
// taps at those centres register on the correct shape button.
// ─────────────────────────────────────────────────────────────────────────────

namespace {
// Returns the pixel X-centre of button i using the NDC path (mirrors gesture_system.cpp).
float ndc_btn_cx(int i) {
    const float btn_w       = constants::BUTTON_W_N       * static_cast<float>(constants::SCREEN_W);
    const float btn_spacing = constants::BUTTON_SPACING_N  * static_cast<float>(constants::SCREEN_W);
    const float btn_area_x  = (static_cast<float>(constants::SCREEN_W)
                                - 3.0f * btn_w - 2.0f * btn_spacing) / 2.0f;
    return btn_area_x + static_cast<float>(i) * (btn_w + btn_spacing) + btn_w / 2.0f;
}

// Returns the pixel Y-centre of the button row using the NDC path.
float ndc_btn_cy() {
    const float btn_h = constants::BUTTON_H_N * static_cast<float>(constants::SCREEN_H);
    const float btn_y = constants::BUTTON_Y_N  * static_cast<float>(constants::SCREEN_H);
    return btn_y + btn_h / 2.0f;
}

// Returns the pixel X-centre of button i using the legacy pixel constants.
float pixel_btn_cx(int i) {
    const float btn_area_x = (static_cast<float>(constants::SCREEN_W)
                               - 3.0f * constants::BUTTON_W
                               - 2.0f * constants::BUTTON_SPACING) / 2.0f;
    return btn_area_x
        + static_cast<float>(i) * (constants::BUTTON_W + constants::BUTTON_SPACING)
        + constants::BUTTON_W / 2.0f;
}
}  // anonymous namespace

TEST_CASE("ndc: button centres from NDC match pixel-constant centres", "[ndc][gesture]") {
    using Catch::Matchers::WithinAbs;
    for (int i = 0; i < 3; ++i) {
        CHECK_THAT(ndc_btn_cx(i), WithinAbs(pixel_btn_cx(i), 0.01f));
    }
    const float pixel_cy = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    CHECK_THAT(ndc_btn_cy(), WithinAbs(pixel_cy, 0.01f));
}

TEST_CASE("ndc: tap at NDC-derived button centre hits Circle", "[ndc][gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_y  = ndc_btn_cy();
    input.end_x    = ndc_btn_cx(0);
    input.end_y    = ndc_btn_cy();
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Circle);
}

TEST_CASE("ndc: tap at NDC-derived button centre hits Square", "[ndc][gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_y  = ndc_btn_cy();
    input.end_x    = ndc_btn_cx(1);
    input.end_y    = ndc_btn_cy();
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Square);
}

TEST_CASE("ndc: tap at NDC-derived button centre hits Triangle", "[ndc][gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_y  = ndc_btn_cy();
    input.end_x    = ndc_btn_cx(2);
    input.end_y    = ndc_btn_cy();
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Triangle);
}

TEST_CASE("ndc: tap at zone boundary (exactly zone_y) enters button zone", "[ndc][gesture]") {
    // A tap starting exactly at the swipe/button boundary should be treated as
    // a button-zone tap (start_y >= zone_y).
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    float zone_y = static_cast<float>(constants::SCREEN_H) * constants::SWIPE_ZONE_SPLIT;
    input.touch_up = true;
    input.start_y  = zone_y;               // exactly on boundary → button zone
    input.end_x    = ndc_btn_cx(0);        // aim at Circle
    input.end_y    = zone_y;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    // The gesture is None (button zone does not produce a swipe)
    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::None);
}

TEST_CASE("ndc: swipe just above zone boundary is classified as swipe zone", "[ndc][gesture]") {
    // 1 pixel above zone_y → swipe zone; use a clear horizontal swipe so the
    // result is unique to swipe-zone handling (not just a missed button tap).
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    float zone_y = static_cast<float>(constants::SCREEN_H) * constants::SWIPE_ZONE_SPLIT;
    input.touch_up = true;
    input.start_x  = 0.0f;
    input.start_y  = zone_y - 1.0f;                             // just above → swipe zone
    input.end_x    = static_cast<float>(constants::SCREEN_W);   // full-width swipe right
    input.end_y    = zone_y - 1.0f;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeRight);
    CHECK_FALSE(reg.ctx().get<ShapeButtonEvent>().pressed);
}
