#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── Helper: set up registry in LevelSelect phase ─────────────────────
static entt::registry make_level_select_registry() {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 1.0f;  // past the transition debounce guard
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

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 490.0f);

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

    // No actions in queue
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
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;                  // start elsewhere

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 250.0f);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);
}

TEST_CASE("level_select — touch card 1", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 490.0f);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 1);
}

TEST_CASE("level_select — touch card 2", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 730.0f);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 2);
}

TEST_CASE("level_select — touch outside cards: no change", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 30.0f, 300.0f);

    level_select_system(reg, 0.016f);
    CHECK(lss.selected_level == 0);          // unchanged
    CHECK(lss.selected_difficulty == 1);     // unchanged
    CHECK_FALSE(lss.confirmed);
}

// ═══════════════════════════════════════════════════════════════════════
// Touch — difficulty button selection
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — touch difficulty easy", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 180.0f, 340.0f);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);
}

TEST_CASE("level_select — touch difficulty medium", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level      = 0;
    lss.selected_difficulty = 0;             // start at easy

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 340.0f);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 1);
}

TEST_CASE("level_select — touch difficulty hard", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 540.0f, 340.0f);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);
}

// ═══════════════════════════════════════════════════════════════════════
// Touch — START button
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — touch START button confirms", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 1080.0f);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.confirmed);
}

TEST_CASE("level_select — touch outside START button: no confirm",
          "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 1200.0f);

    level_select_system(reg, 0.016f);
    REQUIRE_FALSE(lss.confirmed);
}

// ═══════════════════════════════════════════════════════════════════════
// Keyboard navigation (via ActionQueue — platform-agnostic)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — Go Up cycles level up", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.go(Direction::Up);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // 1 → 0
}

TEST_CASE("level_select — Go Down cycles level down", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.go(Direction::Down);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 1);        // 0 → 1
}

TEST_CASE("level_select — Go Left cycles difficulty left", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    // default selected_difficulty = 1 (medium)

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.go(Direction::Left);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);   // 1 → 0
}

TEST_CASE("level_select — Go Right cycles difficulty right", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    // default selected_difficulty = 1 (medium)

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.go(Direction::Right);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);   // 1 → 2
}

TEST_CASE("level_select — Tap Confirm confirms", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Confirm);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.confirmed);
}

TEST_CASE("level_select — Go Up wraps level 0 → 2", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.go(Direction::Up);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 2);        // (0 - 1 + 3) % 3 = 2
}

TEST_CASE("level_select — Go Down wraps level 2 → 0", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 2;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.go(Direction::Down);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // (2 + 1) % 3 = 0
}

TEST_CASE("level_select — Go Left wraps difficulty 0 → 2", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 0;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.go(Direction::Left);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);   // (0 - 1 + 3) % 3 = 2
}

TEST_CASE("level_select — Go Right wraps difficulty 2 → 0", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 2;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.go(Direction::Right);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);   // (2 + 1) % 3 = 0
}

TEST_CASE("level_select — desktop mouse click full pipeline", "[level_select][desktop]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 1.0f; // past the 0.2s guard
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;
    lss.confirmed = false;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 780.0f);

    // Run systems in exact execution order
    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    // Card 2 should be selected
    REQUIRE(lss.selected_level == 2);
}

TEST_CASE("level_select — mouse click on START triggers confirmed", "[level_select][desktop]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 1.0f;
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.confirmed = false;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 1080.0f);

    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    REQUIRE(lss.confirmed == true);
}

TEST_CASE("level_select — title click should NOT leak into level select", "[level_select][regression]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    gs.phase_timer = 1.0f;
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;
    lss.confirmed = false;

    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, 360.0f, 600.0f);

    // Tick 1: Title → sets transition_pending
    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    // Tick 2: transition fires → phase = LevelSelect.
    // ActionQueue still has the same action (not cleared between fixed ticks).
    // level_select_system should debounce.
    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    REQUIRE(gs.phase == GamePhase::LevelSelect);

    CHECK(lss.selected_level == 0);
    CHECK(lss.confirmed == false);
}
