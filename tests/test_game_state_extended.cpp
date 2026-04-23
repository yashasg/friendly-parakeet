#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── game_state_system: SongComplete transitions ──────────────

TEST_CASE("game_state: song complete when song finished and no obstacles", "[gamestate]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.playing = false;
    // No obstacle entities → should trigger SongComplete

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::SongComplete);
}

TEST_CASE("game_state: song complete waits for obstacles to clear", "[gamestate]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.playing = false;

    // Create an unscored obstacle
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, 0.0f, 0.0f);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: song complete ignores scored obstacles", "[gamestate]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.playing = false;

    // Only a scored obstacle remains → should transition to SongComplete
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<Position>(obs, 0.0f, 0.0f);

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::SongComplete);
}

TEST_CASE("game_state: enter_song_complete updates high score", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 8000;
    score.high_score = 5000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::SongComplete;

    game_state_system(reg, 0.016f);

    CHECK(score.high_score == 8000);
    CHECK(gs.phase == GamePhase::SongComplete);
}

TEST_CASE("game_state: enter_song_complete preserves higher high_score", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 1000;
    score.high_score = 9000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::SongComplete;

    game_state_system(reg, 0.016f);

    CHECK(score.high_score == 9000);
}

TEST_CASE("game_state: song_complete button choice restart", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::SongComplete;
    gs.phase_timer = 1.0f;
    gs.end_choice = EndScreenChoice::Restart;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Playing);
    CHECK(gs.end_choice == EndScreenChoice::None);
}

TEST_CASE("game_state: song_complete button choice level_select", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::SongComplete;
    gs.phase_timer = 1.0f;
    gs.end_choice = EndScreenChoice::LevelSelect;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::LevelSelect);
}

TEST_CASE("game_state: song_complete button choice main_menu", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::SongComplete;
    gs.phase_timer = 1.0f;
    gs.end_choice = EndScreenChoice::MainMenu;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Title);
}

TEST_CASE("game_state: song_complete ignores choice during delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::SongComplete;
    gs.phase_timer = 0.2f;  // < 0.5s delay
    gs.end_choice = EndScreenChoice::Restart;

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: transition to LevelSelect resets confirmed", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.confirmed = true;

    gs.transition_pending = true;
    gs.next_phase = GamePhase::LevelSelect;

    game_state_system(reg, 0.016f);

    CHECK(gs.phase == GamePhase::LevelSelect);
    CHECK_FALSE(lss.confirmed);
}

TEST_CASE("game_state: transition to Title sets phase and resets timer", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 5.0f;
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Title;

    game_state_system(reg, 0.016f);

    CHECK(gs.phase == GamePhase::Title);
    CHECK(gs.phase_timer == 0.0f);
}

TEST_CASE("game_state: level select confirmed triggers playing transition", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 0.5f;  // past 0.2s guard
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.confirmed = true;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Playing);
    CHECK_FALSE(lss.confirmed);
}

TEST_CASE("game_state: level select ignores confirmed during delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 0.1f;  // within 0.2s delay
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.confirmed = true;

    game_state_system(reg, 0.016f);

    // confirmed still true, no transition yet
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: game_over restart button triggers playing", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.5f;
    gs.end_choice = EndScreenChoice::Restart;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Playing);
}

TEST_CASE("game_state: game_over main_menu button triggers title", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.5f;
    gs.end_choice = EndScreenChoice::MainMenu;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Title);
}

TEST_CASE("game_state: title position tap triggers level_select", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    auto& eq = reg.ctx().get<EventQueue>();
    // Simulate a tap → Confirm menu button press (title screen has full-screen confirm)
    auto btn = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::Title);
    eq.push_press(btn);

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::LevelSelect);
}

TEST_CASE("game_state: transition_pending consumed on execution", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Paused;

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
}

