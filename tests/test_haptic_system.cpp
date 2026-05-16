#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "systems/haptics.h"
#include "systems/audio_events.h"
#include "entities/settings.h"
#include "components/game_state.h"
#include "components/player.h"
#include "components/transform.h"
#include "systems/input_events.h"
#include "components/obstacle.h"
#include "constants.h"

// ── haptic_system integration tests ──────────────────────────────────────────

TEST_CASE("haptic_system: drains enqueued PlayHapticEvents without crashing", "[haptic]") {
    auto reg = make_registry();
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.enqueue<PlayHapticEvent>({HapticEvent::DeathCrash});
    disp.enqueue<PlayHapticEvent>({HapticEvent::RetryTap});

    // Must not crash; platform stub is a no-op.
    haptic_system(reg);

    // After drain, queue is empty.
    auto cap = drain_haptic_events(reg);
    CHECK(cap.count == 0);
}

TEST_CASE("haptic_system: no-op when queue is empty", "[haptic]") {
    auto reg = make_registry();
    const auto entity_count_before = reg.storage<entt::entity>().size();

    haptic_system(reg);

    CHECK(reg.storage<entt::entity>().size() == entity_count_before);
    auto cap = drain_haptic_events(reg);
    CHECK(cap.count == 0);
}

TEST_CASE("haptic_system: safe when dispatcher absent from context", "[haptic]") {
    entt::registry reg;  // bare registry — no dispatcher
    REQUIRE_FALSE(reg.ctx().contains<entt::dispatcher>());

    haptic_system(reg);

    CHECK_FALSE(reg.ctx().contains<entt::dispatcher>());
    SUCCEED("haptic_system handled missing dispatcher without throwing");
}

// ── game_state_system haptic wiring ──────────────────────────────────────────

TEST_CASE("game state haptic: DeathCrash enqueued on game over", "[haptic]") {
    auto reg = make_registry();
    set_test_phase(reg, GamePhase::Playing);
    reg.ctx().get<GameState>().transition_pending = true;
    reg.ctx().get<GameState>().next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto cap = drain_haptic_events(reg);
    bool found_crash = false;
    for (int i = 0; i < cap.count; ++i)
        if (cap.buf[i] == HapticEvent::DeathCrash) found_crash = true;
    CHECK(found_crash);
}

TEST_CASE("game state haptic: NewHighScore enqueued when score exceeds high score on game over", "[haptic]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 1000;
    current.value = 500;
    reg.ctx().get<GameState>().transition_pending = true;
    reg.ctx().get<GameState>().next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto cap = drain_haptic_events(reg);
    bool found_hs = false;
    for (int i = 0; i < cap.count; ++i)
        if (cap.buf[i] == HapticEvent::NewHighScore) found_hs = true;
    CHECK(found_hs);
}

TEST_CASE("game state haptic: NewHighScore NOT enqueued when no new high score on game over", "[haptic]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 400;
    current.value = 500;
    reg.ctx().get<GameState>().transition_pending = true;
    reg.ctx().get<GameState>().next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto cap = drain_haptic_events(reg);
    bool found_hs = false;
    for (int i = 0; i < cap.count; ++i)
        if (cap.buf[i] == HapticEvent::NewHighScore) found_hs = true;
    CHECK_FALSE(found_hs);
}

TEST_CASE("game state haptic: DeathCrash still enqueued; listener gates hardware when haptics_enabled=false", "[haptic]") {
    // Gating moved from push site → listener (haptic_handle_play).
    // The event is always enqueued; the listener skips platform::haptics::trigger when disabled.
    // This test verifies the event IS enqueued (semantic intent is recorded) and that
    // haptic_system completes without crash when the listener gates the hardware call.
    auto reg = make_registry();
    settings_state(reg).haptics_enabled = false;
    reg.ctx().get<GameState>().transition_pending = true;
    reg.ctx().get<GameState>().next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    // Event IS enqueued (intent recorded).
    auto cap = drain_haptic_events(reg);
    bool found_crash = false;
    for (int i = 0; i < cap.count; ++i)
        if (cap.buf[i] == HapticEvent::DeathCrash) found_crash = true;
    CHECK(found_crash);
    // haptic_system should also complete without crash (listener gates hardware call).
    // (Already verified above since drain_haptic_events calls update, triggering the listener.)
}

TEST_CASE("game state haptic: RetryTap enqueued when Restart pressed on end screen", "[haptic]") {
    auto reg = make_registry();
    set_test_phase(reg, GamePhase::GameOver);
    reg.ctx().get<GameState>().phase_timer = 1.0f;

    auto btn = make_menu_button(reg, MenuActionKind::Restart);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    auto cap = drain_haptic_events(reg);
    bool found_retry = false;
    for (int i = 0; i < cap.count; ++i)
        if (cap.buf[i] == HapticEvent::RetryTap) found_retry = true;
    CHECK(found_retry);
}

TEST_CASE("game state haptic: UIButtonTap enqueued for non-Restart end screen button", "[haptic]") {
    auto reg = make_registry();
    set_test_phase(reg, GamePhase::GameOver);
    reg.ctx().get<GameState>().phase_timer = 1.0f;

    auto btn = make_menu_button(reg, MenuActionKind::GoMainMenu);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    auto cap = drain_haptic_events(reg);
    bool found_ui = false;
    for (int i = 0; i < cap.count; ++i)
        if (cap.buf[i] == HapticEvent::UIButtonTap) found_ui = true;
    CHECK(found_ui);
}

TEST_CASE("game state haptic: haptic_system no-crash when haptics_enabled=false and RetryTap enqueued", "[haptic]") {
    // Listener gates platform call; system must complete cleanly.
    auto reg = make_registry();
    settings_state(reg).haptics_enabled = false;
    set_test_phase(reg, GamePhase::GameOver);
    reg.ctx().get<GameState>().phase_timer = 1.0f;

    auto btn = make_menu_button(reg, MenuActionKind::Restart);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);
    haptic_system(reg);  // must not crash
}
