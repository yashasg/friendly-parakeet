#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "systems/game_state_system.h"
#include "test_helpers.h"
#include "ui/screen_controllers/tutorial_screen_controller.h"

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

TEST_CASE("game_state: energy depletion beats song complete after playback finishes",
          "[gamestate][issue755]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.playing = false;
    reg.ctx().get<EnergyState>().energy = 0.0f;
    auto& game_over = reg.ctx().get<GameOverState>();
    game_over.cause = DeathCause::None;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
    CHECK(game_over.cause == DeathCause::EnergyDepleted);
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
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, 0.0f}});

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
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, 0.0f}});

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: enter_song_complete updates high score", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 8000;
    current.value = 5000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::SongComplete;

    game_state_system(reg, 0.016f);

    CHECK(current.value == 8000);
    CHECK(gs.phase == GamePhase::SongComplete);
    const auto& terminal = reg.ctx().get<TerminalResultState>();
    CHECK(terminal.new_best);
    CHECK(terminal.previous_best == 5000);
}

TEST_CASE("game_state: enter_song_complete preserves higher high_score", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 1000;
    current.value = 9000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::SongComplete;

    game_state_system(reg, 0.016f);

    CHECK(current.value == 9000);
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

TEST_CASE("game_state: terminal to LevelSelect hides stale world renderables",
          "[gamestate][render][regression][issue921]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.confirmed = true;

    auto stale = reg.create();
    reg.emplace<ModelTransform>(stale);
    reg.emplace<TagWorldPass>(stale);

    gs.phase = GamePhase::GameOver;
    gs.transition_pending = true;
    gs.next_phase = GamePhase::LevelSelect;

    game_state_system(reg, 0.016f);

    CHECK(gs.phase == GamePhase::LevelSelect);
    CHECK_FALSE(lss.confirmed);
    REQUIRE(reg.valid(stale));
    CHECK(reg.all_of<ModelTransform, TagWorldPass>(stale));
    CHECK_FALSE(game_render_should_draw_world_meshes(gs.phase));
}

TEST_CASE("game_state: Settings hides stale world renderables",
          "[gamestate][render][regression][issue966]") {
    CHECK(game_render_should_draw_world_meshes(GamePhase::Playing));
    CHECK(game_render_should_draw_world_meshes(GamePhase::Paused));
    CHECK(game_render_should_draw_world_meshes(GamePhase::GameOver));
    CHECK(game_render_should_draw_world_meshes(GamePhase::SongComplete));
    CHECK_FALSE(game_render_should_draw_world_meshes(GamePhase::Title));
    CHECK_FALSE(game_render_should_draw_world_meshes(GamePhase::LevelSelect));
    CHECK_FALSE(game_render_should_draw_world_meshes(GamePhase::Settings));
    CHECK_FALSE(game_render_should_draw_world_meshes(GamePhase::Tutorial));
}

TEST_CASE("wasm smoke lane marker state is registry-owned and reset with runtime scratch",
          "[gamestate][wasm][regression][issue973]") {
    auto first = make_registry();
    auto second = make_registry();

    auto& first_marker = first.ctx().get<WasmSmokeLaneMarkerState>();
    auto& second_marker = second.ctx().get<WasmSmokeLaneMarkerState>();
    first_marker.last_lane = 2;

    CHECK(second_marker.last_lane == -1);

    runtime_system_scratch_init(first);

    CHECK(first.ctx().get<WasmSmokeLaneMarkerState>().last_lane == -1);
    CHECK(second.ctx().get<WasmSmokeLaneMarkerState>().last_lane == -1);
}

TEST_CASE("game_state: transition clears in-flight pointer capture", "[gamestate][input]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    auto& input = reg.ctx().get<InputState>();
    auto& priv = reg.ctx().get<InputSystemPrivate>();

    input.touch_down = true;
    input.touch_up = false;
    input.touching = true;
    input.active_source = InputSource::Mouse;
    priv.suppress_mouse_release = true;
    input.duration = 0.25f;

    gs.transition_pending = true;
    gs.next_phase = GamePhase::LevelSelect;

    game_state_system(reg, 0.016f);

    CHECK_FALSE(input.touch_down);
    CHECK_FALSE(input.touch_up);
    CHECK_FALSE(input.touching);
    CHECK(input.active_source == InputSource::None);
    CHECK_FALSE(priv.suppress_mouse_release);
    CHECK(input.duration == 0.0f);
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

TEST_CASE("game_state: level select confirmed starts tutorial when FTUE is incomplete", "[gamestate][ftue][issue882]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 0.5f;  // past 0.2s guard
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.confirmed = true;
    settings_state(reg).ftue_run_count = 0;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Tutorial);
    CHECK_FALSE(lss.confirmed);
}

TEST_CASE("game_state: level select confirmed triggers playing when FTUE is complete",
          "[gamestate][ftue][issue882]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::LevelSelect;
    gs.phase_timer = 0.5f;  // past 0.2s guard
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.confirmed = true;
    settings_state(reg).ftue_run_count = 1;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Playing);
    CHECK_FALSE(lss.confirmed);
}

TEST_CASE("tutorial: continue marks FTUE complete and requests gameplay",
          "[gamestate][ftue][issue882]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Tutorial;

    auto& settings = settings_state(reg);
    settings.ftue_run_count = 0;
    auto& persistence = settings_persistence(reg);
    persistence.path.clear();
    persistence.dirty = false;

    tutorial_screen_continue(reg);

    CHECK(settings::ftue_complete(settings));
    CHECK(persistence.dirty);
    CHECK(persistence.last_save.status == persistence::Status::PathUnavailable);
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Playing);
}

TEST_CASE("tutorial: menu confirm marks FTUE complete and requests gameplay",
          "[gamestate][ftue][input][issue882]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Tutorial;
    gs.phase_timer = 0.5f;

    auto& settings = settings_state(reg);
    settings.ftue_run_count = 0;
    auto& persistence = settings_persistence(reg);
    persistence.path.clear();
    persistence.dirty = false;

    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({
        ButtonPressKind::Menu, Shape::Circle, MenuActionKind::Confirm, 0});
    game_state_system(reg, 0.016f);

    const auto& saved_settings = settings_state(reg);
    const auto& saved_persistence = settings_persistence(reg);
    CHECK(settings::ftue_complete(saved_settings));
    CHECK(saved_persistence.dirty);
    CHECK(saved_persistence.last_save.status == persistence::Status::PathUnavailable);
    CHECK_FALSE(gs.transition_pending);
    CHECK(gs.phase == GamePhase::Playing);
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

TEST_CASE("game_state: game_over level_select button triggers level_select", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.5f;
    gs.end_choice = EndScreenChoice::LevelSelect;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::LevelSelect);
    CHECK(gs.end_choice == EndScreenChoice::None);
}

TEST_CASE("game_state: game_over ignores choice at readiness boundary", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.4f;
    gs.end_choice = EndScreenChoice::Restart;

    game_state_system(reg, 0.0f);

    CHECK_FALSE(gs.transition_pending);
    CHECK(gs.end_choice == EndScreenChoice::Restart);
}

TEST_CASE("game_state: game_over ignores missing end choice", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.5f;
    gs.end_choice = EndScreenChoice::None;

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
    CHECK(gs.end_choice == EndScreenChoice::None);
}

TEST_CASE("game_state: game_over restart enters fresh play session on next tick", "[gamestate][play_session]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.5f;
    gs.end_choice = EndScreenChoice::Restart;

    auto stale = reg.create();
    reg.emplace<ObstacleTag>(stale);
    reg.emplace<WorldTransform>(stale, WorldTransform{{0.0f, 0.0f}});
    reg.ctx().get<ScoreState>().score = 3210;

    game_state_system(reg, 0.016f);
    REQUIRE(gs.transition_pending);
    REQUIRE(gs.next_phase == GamePhase::Playing);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
    CHECK(gs.phase == GamePhase::Playing);
    CHECK_FALSE(reg.valid(stale));
    CHECK_FALSE(reg.view<PlayerTag>().empty());
    CHECK(reg.ctx().get<ScoreState>().score == 0);
}

TEST_CASE("game_state: title position tap triggers level_select", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    // Simulate a tap → Confirm menu button press (title screen has full-screen confirm)
    auto btn = make_menu_button(reg, MenuActionKind::Confirm);
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

    // Proxy for title gear click path: title_screen_controller sets
    // transition_pending + next_phase=Settings, then fixed-step consumes it.
    game_state_system(reg, 0.016f);
    REQUIRE(gs.phase == GamePhase::Settings);
    CHECK_FALSE(gs.transition_pending);
    CHECK(gs.phase_timer == 0.0f);
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
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 12345;
    current.value = 10000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::SongComplete;

    game_state_system(reg, 0.016f);

    // score.score must NOT be zeroed — it is the final value rendered on the results screen
    CHECK(score.score == 12345);
    CHECK(gs.phase == GamePhase::SongComplete);
}
