#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "systems/ui_button_spawner.h"

// ── Helper: set up registry in LevelSelect phase ─────────────────────
static entt::registry make_level_select_registry() {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 1.0f;  // past the transition debounce guard
    // Spawn level-select UI entities (cards, diff buttons, start)
    spawn_level_select_buttons(reg);
    return reg;
}

// Helpers to find button entities by kind/index
static entt::entity find_menu_button(entt::registry& reg, MenuActionKind kind, uint8_t index = 0) {
    auto view = reg.view<MenuButtonTag, MenuAction>();
    for (auto [e, ma] : view.each()) {
        if (ma.kind == kind && ma.index == index) return e;
    }
    return entt::null;
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

    auto& eq = reg.ctx().get<EventQueue>();
    auto btn = make_menu_button(reg, MenuActionKind::SelectLevel, GamePhase::LevelSelect, 1);
    eq.push_press(btn);

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
// Touch — card selection (via ButtonPressEvent)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — touch card 0", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;                  // start elsewhere

    auto& eq = reg.ctx().get<EventQueue>();
    auto card = find_menu_button(reg, MenuActionKind::SelectLevel, 0);
    REQUIRE((card != entt::null));
    eq.push_press(card);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);
}

TEST_CASE("level_select — touch card 1", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& eq = reg.ctx().get<EventQueue>();
    auto card = find_menu_button(reg, MenuActionKind::SelectLevel, 1);
    REQUIRE((card != entt::null));
    eq.push_press(card);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 1);
}

TEST_CASE("level_select — touch card 2", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& eq = reg.ctx().get<EventQueue>();
    auto card = find_menu_button(reg, MenuActionKind::SelectLevel, 2);
    REQUIRE((card != entt::null));
    eq.push_press(card);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 2);
}

TEST_CASE("level_select — no button press: no change", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    // Push an input event but no button press
    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_input(InputType::Tap, 30.0f, 300.0f);

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

    auto& eq = reg.ctx().get<EventQueue>();
    auto diff = find_menu_button(reg, MenuActionKind::SelectDiff, 0);
    REQUIRE((diff != entt::null));
    eq.push_press(diff);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);
}

TEST_CASE("level_select — touch difficulty medium", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level      = 0;
    lss.selected_difficulty = 0;             // start at easy

    auto& eq = reg.ctx().get<EventQueue>();
    auto diff = find_menu_button(reg, MenuActionKind::SelectDiff, 1);
    REQUIRE((diff != entt::null));
    eq.push_press(diff);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 1);
}

TEST_CASE("level_select — touch difficulty hard", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto& eq = reg.ctx().get<EventQueue>();
    auto diff = find_menu_button(reg, MenuActionKind::SelectDiff, 2);
    REQUIRE((diff != entt::null));
    eq.push_press(diff);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);
}

// ═══════════════════════════════════════════════════════════════════════
// Touch — START button
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — touch START button confirms", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& eq = reg.ctx().get<EventQueue>();
    auto start = find_menu_button(reg, MenuActionKind::Confirm, 0);
    REQUIRE((start != entt::null));
    eq.push_press(start);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.confirmed);
}

TEST_CASE("level_select — no START press: no confirm",
          "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    // Empty event queue
    level_select_system(reg, 0.016f);
    REQUIRE_FALSE(lss.confirmed);
}

// ═══════════════════════════════════════════════════════════════════════
// Keyboard navigation (via EventQueue — platform-agnostic)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — Go Up cycles level up", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_go(Direction::Up);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // 1 → 0
}

TEST_CASE("level_select — Go Down cycles level down", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_go(Direction::Down);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 1);        // 0 → 1
}

TEST_CASE("level_select — Go Left cycles difficulty left", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    // default selected_difficulty = 1 (medium)

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_go(Direction::Left);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);   // 1 → 0
}

TEST_CASE("level_select — Go Right cycles difficulty right", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    // default selected_difficulty = 1 (medium)

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_go(Direction::Right);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);   // 1 → 2
}

TEST_CASE("level_select — Tap Confirm confirms", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto& eq = reg.ctx().get<EventQueue>();
    auto start = find_menu_button(reg, MenuActionKind::Confirm, 0);
    REQUIRE((start != entt::null));
    eq.push_press(start);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.confirmed);
}

TEST_CASE("level_select — Go Up wraps level 0 → 2", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_go(Direction::Up);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 2);        // (0 - 1 + 3) % 3 = 2
}

TEST_CASE("level_select — Go Down wraps level 2 → 0", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 2;

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_go(Direction::Down);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // (2 + 1) % 3 = 0
}

TEST_CASE("level_select — Go Left wraps difficulty 0 → 2", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 0;

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_go(Direction::Left);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);   // (0 - 1 + 3) % 3 = 2
}

TEST_CASE("level_select — Go Right wraps difficulty 2 → 0", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 2;

    auto& eq = reg.ctx().get<EventQueue>();
    eq.push_go(Direction::Right);
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);   // (2 + 1) % 3 = 0
}

TEST_CASE("level_select — desktop mouse click full pipeline", "[level_select][desktop]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 1.0f; // past the 0.2s guard
    spawn_level_select_buttons(reg);
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;
    lss.confirmed = false;

    // Tap on card 2 (via hit_test_system → press event)
    auto& eq = reg.ctx().get<EventQueue>();
    auto card2 = find_menu_button(reg, MenuActionKind::SelectLevel, 2);
    REQUIRE((card2 != entt::null));
    eq.push_press(card2);

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
    spawn_level_select_buttons(reg);
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.confirmed = false;

    auto& eq = reg.ctx().get<EventQueue>();
    auto start = find_menu_button(reg, MenuActionKind::Confirm, 0);
    REQUIRE((start != entt::null));
    eq.push_press(start);

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

    auto& eq = reg.ctx().get<EventQueue>();
    // Simulate a title-screen confirm press
    auto btn = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::Title);
    eq.push_press(btn);

    // Tick 1: Title → sets transition_pending
    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    // Tick 2: transition fires → phase = LevelSelect.
    // EventQueue still has the same press (not cleared between fixed ticks).
    // level_select_system should debounce.
    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    REQUIRE(gs.phase == GamePhase::LevelSelect);

    CHECK(lss.selected_level == 0);
    CHECK(lss.confirmed == false);
}
