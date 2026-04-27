#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <algorithm>
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
// NDC-derived button positions
// Verify that the button centres computed from NDC constants match the
// raw pixel constants. Gesture classification is now internal to input_system
// (which calls raylib and can't be unit-tested), so we only verify the
// math here.
// ─────────────────────────────────────────────────────────────────────────────

namespace {
// Returns the pixel X-centre of button i using the NDC path (mirrors input_system.cpp).
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

TEST_CASE("ndc: button centres from NDC match pixel-constant centres", "[ndc]") {
    using Catch::Matchers::WithinAbs;
    for (int i = 0; i < 3; ++i) {
        CHECK_THAT(ndc_btn_cx(i), WithinAbs(pixel_btn_cx(i), 0.01f));
    }
    const float pixel_cy = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    CHECK_THAT(ndc_btn_cy(), WithinAbs(pixel_cy, 0.01f));
}

TEST_CASE("ndc: zone boundary constant is 0.80", "[ndc]") {
    CHECK(constants::SWIPE_ZONE_SPLIT == 0.80f);
    float zone_y = static_cast<float>(constants::SCREEN_H) * constants::SWIPE_ZONE_SPLIT;
    CHECK(zone_y > 0.0f);
    CHECK(zone_y < static_cast<float>(constants::SCREEN_H));
}

// ─────────────────────────────────────────────────────────────────────────────
// compute_screen_transform idempotency (#241)
// The letterbox formula must be pure (depends only on win/virtual dims).
// Calling it once or twice with the same window size yields identical results.
// This mirrors the math inside compute_screen_transform without raylib.
// ─────────────────────────────────────────────────────────────────────────────

namespace {
// Pure mirror of the letterbox math in compute_screen_transform.
ScreenTransform make_screen_transform(float win_w, float win_h) {
    float scale = std::min(
        win_w / static_cast<float>(constants::SCREEN_W),
        win_h / static_cast<float>(constants::SCREEN_H));
    float dst_w = constants::SCREEN_W * scale;
    float dst_h = constants::SCREEN_H * scale;
    ScreenTransform st;
    st.offset_x = (win_w - dst_w) * 0.5f;
    st.offset_y = (win_h - dst_h) * 0.5f;
    st.scale    = scale;
    return st;
}
}  // anonymous namespace

TEST_CASE("screen_transform: letterbox math is idempotent (#241)",
          "[screen_transform][regression]") {
    // Simulate a 1440×2560 window (2× virtual 720×1280, no bars).
    const float win_w = 1440.0f, win_h = 2560.0f;
    ScreenTransform a = make_screen_transform(win_w, win_h);
    ScreenTransform b = make_screen_transform(win_w, win_h);
    CHECK(a.scale    == b.scale);
    CHECK(a.offset_x == b.offset_x);
    CHECK(a.offset_y == b.offset_y);
    // Scale must be 2.0 and offsets 0 (no letterbar at exact 2× ratio)
    using Catch::Matchers::WithinAbs;
    CHECK_THAT(a.scale,    WithinAbs(2.0f, 1e-5f));
    CHECK_THAT(a.offset_x, WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(a.offset_y, WithinAbs(0.0f, 1e-5f));
}

TEST_CASE("screen_transform: letterbox math produces correct pillarbox offsets (#241)",
          "[screen_transform][regression]") {
    // 1280×1280 window: scale = min(1280/720, 1280/1280) = 1.0;
    // dst_w = 720, offset_x = (1280-720)/2 = 280; dst_h = 1280, offset_y = 0.
    ScreenTransform st = make_screen_transform(1280.0f, 1280.0f);
    using Catch::Matchers::WithinAbs;
    CHECK_THAT(st.scale,    WithinAbs(1.0f,  1e-5f));
    CHECK_THAT(st.offset_x, WithinAbs(280.0f, 1e-5f));
    CHECK_THAT(st.offset_y, WithinAbs(0.0f,  1e-5f));
}
