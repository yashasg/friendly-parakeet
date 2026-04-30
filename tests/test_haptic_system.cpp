#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "components/haptics.h"
#include "util/haptic_queue.h"
#include "util/settings.h"
#include "components/player.h"
#include "components/transform.h"
#include "components/input_events.h"
#include "components/obstacle.h"
#include "constants.h"

// ── HapticQueue unit tests ────────────────────────────────────────────────────

TEST_CASE("haptic_push: emits event when haptics_enabled=true", "[haptic]") {
    HapticQueue hq;
    haptic_push(hq, true, HapticEvent::ShapeShift);
    CHECK(hq.count == 1);
    CHECK(hq.queue[0] == HapticEvent::ShapeShift);
}

TEST_CASE("haptic_push: suppresses event when haptics_enabled=false", "[haptic]") {
    HapticQueue hq;
    haptic_push(hq, false, HapticEvent::ShapeShift);
    CHECK(hq.count == 0);
}

TEST_CASE("haptic_push: does not overflow beyond MAX_QUEUED", "[haptic]") {
    HapticQueue hq;
    for (int i = 0; i < HapticQueue::MAX_QUEUED + 4; ++i)
        haptic_push(hq, true, HapticEvent::UIButtonTap);
    CHECK(hq.count == HapticQueue::MAX_QUEUED);
}

TEST_CASE("haptic_clear: resets count to zero", "[haptic]") {
    HapticQueue hq;
    haptic_push(hq, true, HapticEvent::DeathCrash);
    haptic_push(hq, true, HapticEvent::NewHighScore);
    haptic_clear(hq);
    CHECK(hq.count == 0);
}

// ── haptic_system integration tests ──────────────────────────────────────────

TEST_CASE("haptic_system: drains queue each frame", "[haptic]") {
    auto reg = make_registry();
    auto& hq = reg.ctx().get<HapticQueue>();
    haptic_push(hq, true, HapticEvent::DeathCrash);
    haptic_push(hq, true, HapticEvent::RetryTap);
    CHECK(hq.count == 2);

    haptic_system(reg);
    CHECK(hq.count == 0);
}

TEST_CASE("haptic_system: no-op when queue is empty", "[haptic]") {
    auto reg = make_registry();
    haptic_system(reg);  // must not crash or assert
    CHECK(reg.ctx().get<HapticQueue>().count == 0);
}

TEST_CASE("haptic_system: safe when HapticQueue absent from context", "[haptic]") {
    entt::registry reg;  // bare registry — no HapticQueue
    haptic_system(reg);  // must not crash
}

// ── game_state_system haptic wiring ──────────────────────────────────────────

TEST_CASE("game state haptic: DeathCrash emitted on game over", "[haptic]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Playing;
    // Trigger death via transition
    reg.ctx().get<GameState>().transition_pending = true;
    reg.ctx().get<GameState>().next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto& hq = reg.ctx().get<HapticQueue>();
    bool found_crash = false;
    for (int i = 0; i < hq.count; ++i)
        if (hq.queue[i] == HapticEvent::DeathCrash) found_crash = true;
    CHECK(found_crash);
}

TEST_CASE("game state haptic: NewHighScore emitted when score exceeds high score on game over", "[haptic]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 1000;
    score.high_score = 500;
    reg.ctx().get<GameState>().transition_pending = true;
    reg.ctx().get<GameState>().next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto& hq = reg.ctx().get<HapticQueue>();
    bool found_hs = false;
    for (int i = 0; i < hq.count; ++i)
        if (hq.queue[i] == HapticEvent::NewHighScore) found_hs = true;
    CHECK(found_hs);
}

TEST_CASE("game state haptic: NewHighScore NOT emitted when no new high score on game over", "[haptic]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 400;
    score.high_score = 500;
    reg.ctx().get<GameState>().transition_pending = true;
    reg.ctx().get<GameState>().next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto& hq = reg.ctx().get<HapticQueue>();
    bool found_hs = false;
    for (int i = 0; i < hq.count; ++i)
        if (hq.queue[i] == HapticEvent::NewHighScore) found_hs = true;
    CHECK_FALSE(found_hs);
}

TEST_CASE("game state haptic: DeathCrash suppressed when haptics_enabled=false", "[haptic]") {
    auto reg = make_registry();
    reg.ctx().get<SettingsState>().haptics_enabled = false;
    reg.ctx().get<GameState>().transition_pending = true;
    reg.ctx().get<GameState>().next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto& hq = reg.ctx().get<HapticQueue>();
    bool found_crash = false;
    for (int i = 0; i < hq.count; ++i)
        if (hq.queue[i] == HapticEvent::DeathCrash) found_crash = true;
    CHECK_FALSE(found_crash);
}

TEST_CASE("game state haptic: RetryTap emitted when Restart pressed on end screen", "[haptic]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;
    reg.ctx().get<GameState>().phase_timer = 1.0f;

    auto btn = make_menu_button(reg, MenuActionKind::Restart, GamePhase::GameOver);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    auto& hq = reg.ctx().get<HapticQueue>();
    bool found_retry = false;
    for (int i = 0; i < hq.count; ++i)
        if (hq.queue[i] == HapticEvent::RetryTap) found_retry = true;
    CHECK(found_retry);
}

TEST_CASE("game state haptic: UIButtonTap emitted for non-Restart end screen button", "[haptic]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;
    reg.ctx().get<GameState>().phase_timer = 1.0f;

    auto btn = make_menu_button(reg, MenuActionKind::GoMainMenu, GamePhase::GameOver);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    auto& hq = reg.ctx().get<HapticQueue>();
    bool found_ui = false;
    for (int i = 0; i < hq.count; ++i)
        if (hq.queue[i] == HapticEvent::UIButtonTap) found_ui = true;
    CHECK(found_ui);
}

TEST_CASE("game state haptic: RetryTap suppressed when haptics_enabled=false", "[haptic]") {
    auto reg = make_registry();
    reg.ctx().get<SettingsState>().haptics_enabled = false;
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;
    reg.ctx().get<GameState>().phase_timer = 1.0f;

    auto btn = make_menu_button(reg, MenuActionKind::Restart, GamePhase::GameOver);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().get<HapticQueue>().count == 0);
}
