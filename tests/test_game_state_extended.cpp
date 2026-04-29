#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "components/ui_state.h"

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

TEST_CASE("game_state: song complete proceeds when scored obstacle is destroyed", "[gamestate]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.playing = false;

    // Obstacle scored and destroyed (scoring_system + obstacle_despawn_system both ran) → SongComplete
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ScoredTag>(obs);
    reg.destroy(obs);  // on_destroy<ObstacleTag> fires → counter reaches 0

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::SongComplete);
}

TEST_CASE("game_state: song complete waits for scored obstacle to be destroyed", "[gamestate]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.playing = false;

    // Obstacle scored but still alive (obstacle_despawn_system has not yet destroyed it)
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<Position>(obs, 0.0f, 0.0f);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
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
    CHECK(gs.end_choice == EndScreenChoice::None);
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
    // Simulate a tap → Confirm menu button press (title screen has full-screen confirm)
    auto btn = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::Title);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
    CHECK(gs.phase == GamePhase::LevelSelect);
}

TEST_CASE("title settings navigation: pending Settings transition reaches Settings screen",
          "[gamestate][ui][settings][regression]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    gs.phase_timer = 2.0f;
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Settings;
    reg.ctx().emplace<UIState>();

    // Proxy for title gear click path: title_screen_controller sets
    // transition_pending + next_phase=Settings, then fixed-step consumes it.
    game_state_system(reg, 0.016f);
    REQUIRE(gs.phase == GamePhase::Settings);
    CHECK_FALSE(gs.transition_pending);
    CHECK(gs.phase_timer == 0.0f);

    ui_navigation_system(reg, 0.016f);
    const auto& ui = reg.ctx().get<UIState>();
    CHECK(ui.active == ActiveScreen::Settings);
}

TEST_CASE("game_state: transition_pending consumed on execution", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Paused;

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
}

// ── song_complete: score retention after transition ───────────────────────────
// Regression: "when the song completes score and highscore dont render"
// Keep this focused on gameplay state retention; screen controllers now render
// score labels directly without legacy dynamic source resolution.

TEST_CASE("song_complete: score.score is retained (not zeroed) after enter_song_complete",
          "[gamestate][song_complete]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score      = 12345;
    score.high_score = 10000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::SongComplete;

    game_state_system(reg, 0.016f);

    // score.score must NOT be zeroed — it is the final value rendered on the results screen
    CHECK(score.score == 12345);
    CHECK(gs.phase == GamePhase::SongComplete);
}
