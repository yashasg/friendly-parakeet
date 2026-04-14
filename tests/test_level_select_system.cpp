#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── Helper: set up registry in LevelSelect phase ─────────────────────
static entt::registry make_level_select_registry() {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::LevelSelect;
    return reg;
}

// ═══════════════════════════════════════════════════════════════════════
// Phase guard
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — phase guard: does nothing when not LevelSelect",
          "[level_select]") {
    auto reg = make_registry();
    auto& gs  = reg.ctx().get<GameState>();
    gs.phase  = GamePhase::Playing;          // NOT LevelSelect
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto& input  = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.end_x    = 360.0f;
    input.end_y    = 490.0f;                 // would hit card 1 if active

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // unchanged
    REQUIRE(lss.selected_difficulty == 1);   // unchanged
    REQUIRE_FALSE(lss.confirmed);
}

// ═══════════════════════════════════════════════════════════════════════
// No input → no change
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — no input: state unchanged", "[level_select]") {
    auto reg  = make_level_select_registry();
    auto& lss = reg.ctx().get<LevelSelectState>();

    // No touch_up, no keys — defaults are all false/zero
    level_select_system(reg, 0.016f);

    CHECK(lss.selected_level == 0);
    CHECK(lss.selected_difficulty == 1);
    CHECK_FALSE(lss.confirmed);
}

// ═══════════════════════════════════════════════════════════════════════
// Touch — card selection
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — touch card 0", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;                  // start elsewhere

    input.touch_up = true;
    input.end_x    = 360.0f;
    input.end_y    = 250.0f;                 // card 0: y ∈ [200, 400]

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);
}

TEST_CASE("level_select — touch card 1", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    input.touch_up = true;
    input.end_x    = 360.0f;
    input.end_y    = 490.0f;                 // card 1: y ∈ [440, 640]

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 1);
}

TEST_CASE("level_select — touch card 2", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    input.touch_up = true;
    input.end_x    = 360.0f;
    input.end_y    = 730.0f;                 // card 2: y ∈ [680, 880]

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 2);
}

TEST_CASE("level_select — touch outside cards: no change", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    input.touch_up = true;
    input.end_x    = 30.0f;                  // x < 60 → outside card bounds
    input.end_y    = 300.0f;

    level_select_system(reg, 0.016f);
    CHECK(lss.selected_level == 0);          // unchanged
    CHECK(lss.selected_difficulty == 1);     // unchanged
    CHECK_FALSE(lss.confirmed);
}

// ═══════════════════════════════════════════════════════════════════════
// Touch — difficulty button selection
//   With selected_level = 0:  card_y = 200,  diff_y = 320
//   btn0 (easy):   x ∈ [90, 270],  y ∈ [310, 380]   (incl. 10px pad)
//   btn1 (medium): x ∈ [270, 450], y ∈ [310, 380]
//   btn2 (hard):   x ∈ [450, 630], y ∈ [310, 380]
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — touch difficulty easy", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    input.touch_up = true;
    input.end_x    = 180.0f;                 // btn0: x ∈ [90, 270]
    input.end_y    = 340.0f;                 // diff_y region: y ∈ [310, 380]

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);
}

TEST_CASE("level_select — touch difficulty medium", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level      = 0;
    lss.selected_difficulty = 0;             // start at easy

    input.touch_up = true;
    input.end_x    = 360.0f;                 // btn1: x ∈ [270, 450]
    input.end_y    = 340.0f;

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 1);
}

TEST_CASE("level_select — touch difficulty hard", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    input.touch_up = true;
    input.end_x    = 540.0f;                 // btn2: x ∈ [450, 630]
    input.end_y    = 340.0f;

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);
}

// ═══════════════════════════════════════════════════════════════════════
// Touch — START button
//   x ∈ [200, 520], y ∈ [1040, 1120]  (incl. 10px pad)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — touch START button confirms", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    input.touch_up = true;
    input.end_x    = 360.0f;                 // center of START
    input.end_y    = 1080.0f;                // middle of START y-range

    level_select_system(reg, 0.016f);
    REQUIRE(lss.confirmed);
}

TEST_CASE("level_select — touch outside START button: no confirm",
          "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    input.touch_up = true;
    input.end_x    = 360.0f;
    input.end_y    = 1200.0f;                // below START (y > 1120)

    level_select_system(reg, 0.016f);
    REQUIRE_FALSE(lss.confirmed);
}

// ═══════════════════════════════════════════════════════════════════════
// Keyboard navigation (desktop / web only)
// ═══════════════════════════════════════════════════════════════════════

#ifdef PLATFORM_HAS_KEYBOARD

TEST_CASE("level_select — key W cycles level up", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;

    input.key_w = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // 1 → 0
}

TEST_CASE("level_select — key S cycles level down", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    input.key_s = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 1);        // 0 → 1
}

TEST_CASE("level_select — key A cycles difficulty left", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    // default selected_difficulty = 1 (medium)

    input.key_a = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);   // 1 → 0
}

TEST_CASE("level_select — key D cycles difficulty right", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    // default selected_difficulty = 1 (medium)

    input.key_d = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);   // 1 → 2
}

TEST_CASE("level_select — key Enter confirms", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    input.key_enter = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.confirmed);
}

TEST_CASE("level_select — key W wraps level 0 → 2", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    input.key_w = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 2);        // (0 - 1 + 3) % 3 = 2
}

TEST_CASE("level_select — key S wraps level 2 → 0", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 2;

    input.key_s = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // (2 + 1) % 3 = 0
}

TEST_CASE("level_select — key A wraps difficulty 0 → 2", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 0;

    input.key_a = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);   // (0 - 1 + 3) % 3 = 2
}

TEST_CASE("level_select — key D wraps difficulty 2 → 0", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 2;

    input.key_d = true;
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);   // (2 + 1) % 3 = 0
}

#endif // PLATFORM_HAS_KEYBOARD
