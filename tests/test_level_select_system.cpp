#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
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

    auto btn = make_menu_button(reg, MenuActionKind::SelectLevel, GamePhase::LevelSelect, 1);
    press_button(reg, btn);

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

    auto card = find_menu_button(reg, MenuActionKind::SelectLevel, 0);
    REQUIRE((card != entt::null));
    press_button(reg, card);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);
}

TEST_CASE("level_select — touch card 1", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto card = find_menu_button(reg, MenuActionKind::SelectLevel, 1);
    REQUIRE((card != entt::null));
    press_button(reg, card);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 1);
}

TEST_CASE("level_select — touch card 2", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto card = find_menu_button(reg, MenuActionKind::SelectLevel, 2);
    REQUIRE((card != entt::null));
    press_button(reg, card);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 2);
}

TEST_CASE("level_select — no button press: no change", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    // Push an input event that hits no button
    push_input(reg, InputType::Tap, 30.0f, 300.0f);
    run_input_tier1(reg);

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

    auto diff = find_menu_button(reg, MenuActionKind::SelectDiff, 0);
    REQUIRE((diff != entt::null));
    press_button(reg, diff);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);
}

TEST_CASE("level_select — touch difficulty medium", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level      = 0;
    lss.selected_difficulty = 0;             // start at easy

    auto diff = find_menu_button(reg, MenuActionKind::SelectDiff, 1);
    REQUIRE((diff != entt::null));
    press_button(reg, diff);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 1);
}

TEST_CASE("level_select — touch difficulty hard", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    auto diff = find_menu_button(reg, MenuActionKind::SelectDiff, 2);
    REQUIRE((diff != entt::null));
    press_button(reg, diff);

    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);
}

// ═══════════════════════════════════════════════════════════════════════
// Touch — START button
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — touch START button confirms", "[level_select]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto start = find_menu_button(reg, MenuActionKind::Confirm, 0);
    REQUIRE((start != entt::null));
    press_button(reg, start);

    level_select_system(reg, 0.016f);
    REQUIRE_FALSE(lss.confirmed);
    auto& gs = reg.ctx().get<GameState>();
    REQUIRE(gs.transition_pending);
    REQUIRE(gs.next_phase == GamePhase::Playing);
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
// Keyboard navigation (via GoEvent — platform-agnostic)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("level_select — Go Up cycles level up", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Up});
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // 1 → 0
}

TEST_CASE("level_select — Go Down cycles level down", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Down});
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 1);        // 0 → 1
}

TEST_CASE("level_select — Go Left cycles difficulty left", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    // default selected_difficulty = 1 (medium)

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Left});
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 0);   // 1 → 0
}

TEST_CASE("level_select — Go Right cycles difficulty right", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    // default selected_difficulty = 1 (medium)

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Right});
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);   // 1 → 2
}

TEST_CASE("level_select — Tap Confirm confirms", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();

    auto start = find_menu_button(reg, MenuActionKind::Confirm, 0);
    REQUIRE((start != entt::null));
    press_button(reg, start);
    level_select_system(reg, 0.016f);
    REQUIRE_FALSE(lss.confirmed);
    auto& gs = reg.ctx().get<GameState>();
    REQUIRE(gs.transition_pending);
    REQUIRE(gs.next_phase == GamePhase::Playing);
}

TEST_CASE("level_select — Go Up wraps level 0 → 2", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Up});
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 2);        // (0 - 1 + 3) % 3 = 2
}

TEST_CASE("level_select — Go Down wraps level 2 → 0", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 2;

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Down});
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_level == 0);        // (2 + 1) % 3 = 0
}

TEST_CASE("level_select — Go Left wraps difficulty 0 → 2", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 0;

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Left});
    level_select_system(reg, 0.016f);
    REQUIRE(lss.selected_difficulty == 2);   // (0 - 1 + 3) % 3 = 2
}

TEST_CASE("level_select — Go Right wraps difficulty 2 → 0", "[level_select][keyboard]") {
    auto reg    = make_level_select_registry();
    auto& lss   = reg.ctx().get<LevelSelectState>();
    lss.selected_difficulty = 2;

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Right});
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
    auto card2 = find_menu_button(reg, MenuActionKind::SelectLevel, 2);
    REQUIRE((card2 != entt::null));
    press_button(reg, card2);

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

    auto start = find_menu_button(reg, MenuActionKind::Confirm, 0);
    REQUIRE((start != entt::null));
    press_button(reg, start);

    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    REQUIRE_FALSE(lss.confirmed);
    REQUIRE(gs.transition_pending);
    REQUIRE(gs.next_phase == GamePhase::Playing);
}

TEST_CASE("level_select — title click should NOT leak into level select", "[level_select][regression]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    gs.phase_timer = 1.0f;
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;
    lss.confirmed = false;

    // Simulate a title-screen confirm press
    auto btn = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::Title);
    press_button(reg, btn);

    // Tick 1: Title → sets transition_pending
    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    // Tick 2: transition fires → phase = LevelSelect.
    // ButtonPressEvent queue already drained by tick 1's disp.update<ButtonPressEvent>().
    // level_select_system must not double-process.
    game_state_system(reg, 0.016f);
    level_select_system(reg, 0.016f);

    REQUIRE(gs.phase == GamePhase::LevelSelect);

    CHECK(lss.selected_level == 0);
    CHECK(lss.confirmed == false);
}

// ═══════════════════════════════════════════════════════════════════════
// PR #43 regression — difficulty button Y must be repositioned in the
// same tick that selected_level changes (Go Up/Down or SelectLevel press).
// Before the fix the function returned early, leaving hitboxes at the old
// level's Y coordinate until the *next* frame.
// ═══════════════════════════════════════════════════════════════════════

// Mirrors the local constants in level_select_system.cpp.
namespace {
    constexpr float LS_CARD_START_Y   = 200.0f;
    constexpr float LS_CARD_HEIGHT    = 200.0f;
    constexpr float LS_CARD_GAP       =  40.0f;
    constexpr float LS_DIFF_BTN_Y_OFF = 120.0f;
    constexpr float LS_DIFF_BTN_H     =  50.0f;

    float expected_diff_y(int level) {
        float card_y = LS_CARD_START_Y
                     + static_cast<float>(level) * (LS_CARD_HEIGHT + LS_CARD_GAP);
        return card_y + LS_DIFF_BTN_Y_OFF + LS_DIFF_BTN_H / 2.0f;
    }

    // Collect Y positions of all SelectDiff buttons into a small array.
    // All three buttons share the same Y after repositioning.
    float diff_button_y(entt::registry& reg) {
        float y = -1.0f;
        auto view = reg.view<MenuButtonTag, MenuAction, Position>();
        for (auto [e, ma, pos] : view.each())
            if (ma.kind == MenuActionKind::SelectDiff) { y = pos.y; break; }
        return y;
    }
}  // anonymous namespace

TEST_CASE("level_select: diff button Y updated same tick as Go Up (level 1→0)",
          "[level_select][pr43][regression]") {
    auto reg  = make_level_select_registry();
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;  // start at level 1

    // Pre-condition: run a no-op tick so diff buttons start at level-1 Y.
    level_select_system(reg, 0.016f);
    CHECK_THAT(diff_button_y(reg),
               Catch::Matchers::WithinAbs(expected_diff_y(1), 1.0f));

    // Go Up → selected_level should become 0 AND diff Y should reflect level 0.
    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Up});
    level_select_system(reg, 0.016f);

    REQUIRE(lss.selected_level == 0);
    // Diff button Y must already be at level-0 position in the SAME tick.
    CHECK_THAT(diff_button_y(reg),
               Catch::Matchers::WithinAbs(expected_diff_y(0), 1.0f));
}

TEST_CASE("level_select: diff button Y updated same tick as Go Down (level 0→1)",
          "[level_select][pr43][regression]") {
    auto reg  = make_level_select_registry();
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    level_select_system(reg, 0.016f);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Down});
    level_select_system(reg, 0.016f);

    REQUIRE(lss.selected_level == 1);
    CHECK_THAT(diff_button_y(reg),
               Catch::Matchers::WithinAbs(expected_diff_y(1), 1.0f));
}

TEST_CASE("level_select: diff button Y updated same tick as SelectLevel press",
          "[level_select][pr43][regression]") {
    auto reg  = make_level_select_registry();
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 0;

    // Run a neutral tick to anchor diff buttons at level-0 Y.
    level_select_system(reg, 0.016f);
    CHECK_THAT(diff_button_y(reg),
               Catch::Matchers::WithinAbs(expected_diff_y(0), 1.0f));

    // Press the level-2 card
    auto card2 = find_menu_button(reg, MenuActionKind::SelectLevel, 2);
    REQUIRE((card2 != entt::null));
    press_button(reg, card2);
    level_select_system(reg, 0.016f);

    REQUIRE(lss.selected_level == 2);
    // Diff buttons must be repositioned to level-2 Y in the same tick.
    CHECK_THAT(diff_button_y(reg),
               Catch::Matchers::WithinAbs(expected_diff_y(2), 1.0f));
}
