#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "systems/game_phase_transition.h"
#include "systems/game_state_system.h"
#include "systems/ui_update_system.h"
#include "tags/tags.h"
#include "test_helpers.h"

// ── game_state_system: SongComplete transitions ──────────────

TEST_CASE("game_state: song complete when song finished and no obstacles", "[gamestate]") {
    auto reg = make_rhythm_registry();
    set_test_phase<GamePhasePlayingTag>(reg);
    reg.ctx().emplace<SongFinishedTag>();
    reg.ctx().erase<SongPlayingTag>();
    // No obstacle entities → should trigger SongComplete

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhaseSongCompleteTag>());
}

TEST_CASE("game_state: energy depletion beats song complete after playback finishes",
          "[gamestate][issue755]") {
    auto reg = make_rhythm_registry();
    set_test_phase<GamePhasePlayingTag>(reg);
    reg.ctx().emplace<SongFinishedTag>();
    reg.ctx().erase<SongPlayingTag>();
    reg.ctx().get<EnergyState>().energy = 0.0f;
    REQUIRE(reg.ctx().find<EnergyDepletedDeath>() == nullptr);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhaseGameOverTag>());
    CHECK(reg.ctx().find<EnergyDepletedDeath>() != nullptr);
}

TEST_CASE("game_state: song complete waits for obstacles to clear", "[gamestate]") {
    auto reg = make_rhythm_registry();
    set_test_phase<GamePhasePlayingTag>(reg);
    reg.ctx().emplace<SongFinishedTag>();
    reg.ctx().erase<SongPlayingTag>();

    // Create an unscored obstacle
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldPosition>(obs, WorldPosition{{0.0f, 0.0f}});

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("game_state: song complete proceeds when scored obstacle is destroyed", "[gamestate]") {
    auto reg = make_rhythm_registry();
    set_test_phase<GamePhasePlayingTag>(reg);
    reg.ctx().emplace<SongFinishedTag>();
    reg.ctx().erase<SongPlayingTag>();

    // Obstacle scored and destroyed (scoring_system + obstacle_despawn_system both ran) → SongComplete
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ScoredTag>(obs);
    reg.destroy(obs);  // on_destroy<ObstacleTag> fires → counter reaches 0

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhaseSongCompleteTag>());
}

TEST_CASE("game_state: song complete waits for scored obstacle to be destroyed", "[gamestate]") {
    auto reg = make_rhythm_registry();
    set_test_phase<GamePhasePlayingTag>(reg);
    reg.ctx().emplace<SongFinishedTag>();
    reg.ctx().erase<SongPlayingTag>();

    // Obstacle scored but still alive (obstacle_despawn_system has not yet destroyed it)
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<WorldPosition>(obs, WorldPosition{{0.0f, 0.0f}});

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("game_state: enter_song_complete updates high score", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 8000;
    current.value = 5000;

    request_phase_transition<NextPhaseSongCompleteTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(current.value == 8000);
    CHECK(reg.ctx().contains<GamePhaseSongCompleteTag>());
    REQUIRE(reg.ctx().contains<NewBestRecord>());
    CHECK(reg.ctx().get<NewBestRecord>().previous_best == 5000);
}

TEST_CASE("game_state: enter_song_complete preserves higher high_score", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 1000;
    current.value = 9000;

    request_phase_transition<NextPhaseSongCompleteTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(current.value == 9000);
}

TEST_CASE("game_state: song_complete button choice restart", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseSongCompleteTag>(reg);
    gs.phase_timer = 1.0f;
    reg.ctx().emplace<EndChoiceRestart>();

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
}

TEST_CASE("game_state: song_complete button choice level_select", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseSongCompleteTag>(reg);
    gs.phase_timer = 1.0f;
    reg.ctx().emplace<EndChoiceLevelSelect>();

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhaseLevelSelectTag>());
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
}

TEST_CASE("game_state: song_complete button choice main_menu", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseSongCompleteTag>(reg);
    gs.phase_timer = 1.0f;
    reg.ctx().emplace<EndChoiceMainMenu>();

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhaseTitleTag>());
}

TEST_CASE("game_state: song_complete ignores choice during delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseSongCompleteTag>(reg);
    gs.phase_timer = 0.2f;  // < 0.5s delay
    reg.ctx().emplace<EndChoiceRestart>();

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("game_state: transition to LevelSelect resets confirmed", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});

    request_phase_transition<NextPhaseLevelSelectTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<GamePhaseLevelSelectTag>());
    CHECK_FALSE(reg.ctx().contains<LevelSelectConfirmedTag>());
}

TEST_CASE("game_state: terminal to LevelSelect hides stale world renderables",
          "[gamestate][render][regression][issue921]") {
    auto reg = make_registry();
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});

    auto stale = reg.create();
    reg.emplace<ModelTransform>(stale);
    reg.emplace<TagWorldPass>(stale);

    set_test_phase<GamePhaseGameOverTag>(reg);
    request_phase_transition<NextPhaseLevelSelectTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<GamePhaseLevelSelectTag>());
    CHECK_FALSE(reg.ctx().contains<LevelSelectConfirmedTag>());
    REQUIRE(reg.valid(stale));
    CHECK(reg.all_of<ModelTransform, TagWorldPass>(stale));
    CHECK_FALSE(game_render_should_draw_world_meshes(reg));
}

TEST_CASE("game_state: Settings hides stale world renderables",
          "[gamestate][render][regression][issue966]") {
    auto reg = make_registry();
    sync_game_phase_tags<GamePhasePlayingTag>(reg);
    CHECK(game_render_should_draw_world_meshes(reg));
    sync_game_phase_tags<GamePhasePausedTag>(reg);
    CHECK(game_render_should_draw_world_meshes(reg));
    sync_game_phase_tags<GamePhaseGameOverTag>(reg);
    CHECK(game_render_should_draw_world_meshes(reg));
    sync_game_phase_tags<GamePhaseSongCompleteTag>(reg);
    CHECK(game_render_should_draw_world_meshes(reg));
    sync_game_phase_tags<GamePhaseTitleTag>(reg);
    CHECK_FALSE(game_render_should_draw_world_meshes(reg));
    sync_game_phase_tags<GamePhaseLevelSelectTag>(reg);
    CHECK_FALSE(game_render_should_draw_world_meshes(reg));
    sync_game_phase_tags<GamePhaseSettingsTag>(reg);
    CHECK_FALSE(game_render_should_draw_world_meshes(reg));
    sync_game_phase_tags<GamePhaseTutorialTag>(reg);
    CHECK_FALSE(game_render_should_draw_world_meshes(reg));
}

TEST_CASE("wasm smoke lane marker state is registry-owned and reset with runtime scratch",
          "[gamestate][wasm][regression][issue973]") {
    auto first = make_registry();
    auto second = make_registry();

    // Row absence is the "no lane reported yet" state (Fabian Principle 3 —
    // no sentinel NULL column). make_registry() must leave both fresh.
    CHECK_FALSE(first.ctx().contains<WasmSmokeLastLane>());
    CHECK_FALSE(second.ctx().contains<WasmSmokeLastLane>());

    first.ctx().insert_or_assign(WasmSmokeLastLane{2});

    // Registries are independent: writing to `first` must not leak into `second`.
    CHECK(first.ctx().contains<WasmSmokeLastLane>());
    CHECK(first.ctx().get<WasmSmokeLastLane>().lane == 2);
    CHECK_FALSE(second.ctx().contains<WasmSmokeLastLane>());

    runtime_system_scratch_init(first);

    // Init erases any stale row so the next title update rewrites unconditionally.
    CHECK_FALSE(first.ctx().contains<WasmSmokeLastLane>());
    CHECK_FALSE(second.ctx().contains<WasmSmokeLastLane>());
}

TEST_CASE("game_state: transition clears in-flight pointer capture", "[gamestate][input]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    auto& priv = reg.ctx().get<InputSystemPrivate>();

    input.touch_down = true;
    input.touch_up = false;
    input.touching = true;
    reg.ctx().insert_or_assign(InputSourceMouse{});
    priv.suppress_mouse_release = true;
    input.duration = 0.25f;

    request_phase_transition<NextPhaseLevelSelectTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(input.touch_down);
    CHECK_FALSE(input.touch_up);
    CHECK_FALSE(input.touching);
    CHECK_FALSE(reg.ctx().contains<InputSourceMouse>());
    CHECK_FALSE(reg.ctx().contains<InputSourceTouch>());
    CHECK_FALSE(priv.suppress_mouse_release);
    CHECK(input.duration == 0.0f);
}

TEST_CASE("game_state: transition to Title sets phase and resets timer", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhasePlayingTag>(reg);
    gs.phase_timer = 5.0f;
    request_phase_transition<NextPhaseTitleTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<GamePhaseTitleTag>());
    CHECK(gs.phase_timer == 0.0f);
}

TEST_CASE("game_state: level select confirmed starts tutorial when FTUE is incomplete", "[gamestate][ftue][issue882]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseLevelSelectTag>(reg);
    gs.phase_timer = 0.5f;  // past 0.2s guard
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});
    settings_state(reg).ftue_run_count = 0;

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhaseTutorialTag>());
    CHECK_FALSE(reg.ctx().contains<LevelSelectConfirmedTag>());
}

TEST_CASE("game_state: level select confirmed triggers playing when FTUE is complete",
          "[gamestate][ftue][issue882]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseLevelSelectTag>(reg);
    gs.phase_timer = 0.5f;  // past 0.2s guard
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});
    settings_state(reg).ftue_run_count = 1;

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
    CHECK_FALSE(reg.ctx().contains<LevelSelectConfirmedTag>());
}

TEST_CASE("tutorial: continue marks FTUE complete and requests gameplay",
          "[gamestate][ftue][issue882]") {
    auto reg = make_registry();
    set_test_phase<GamePhaseTutorialTag>(reg);

    auto& settings = settings_state(reg);
    settings.ftue_run_count = 0;
    auto& persistence = settings_persistence(reg);
    persistence.path.clear();
    const auto settings_entity = *reg.view<SettingsTag>().begin();
    reg.remove<SettingsDirtyTag>(settings_entity);

    tutorial_screen_continue(reg);

    CHECK(settings::ftue_complete(settings));
    CHECK(reg.all_of<SettingsDirtyTag>(settings_entity));
    CHECK(persistence.last_save.status == persistence::Status::PathUnavailable);
    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
}

TEST_CASE("tutorial: menu confirm marks FTUE complete and requests gameplay",
          "[gamestate][ftue][input][issue882]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseTutorialTag>(reg);
    gs.phase_timer = 0.5f;

    auto& settings = settings_state(reg);
    settings.ftue_run_count = 0;
    auto& persistence = settings_persistence(reg);
    persistence.path.clear();
    const auto settings_entity = *reg.view<SettingsTag>().begin();
    reg.remove<SettingsDirtyTag>(settings_entity);

    reg.ctx().get<entt::dispatcher>().enqueue<MenuConfirmEvent>({});
    game_state_system(reg, 0.016f);

    const auto& saved_settings = settings_state(reg);
    const auto& saved_persistence = settings_persistence(reg);
    const auto current_settings_entity = *reg.view<SettingsTag>().begin();
    CHECK(settings::ftue_complete(saved_settings));
    CHECK(reg.all_of<SettingsDirtyTag>(current_settings_entity));
    CHECK(saved_persistence.last_save.status == persistence::Status::PathUnavailable);
    CHECK_FALSE(is_phase_transition_pending(reg));
    CHECK(reg.ctx().contains<GamePhasePlayingTag>());
}

TEST_CASE("game_state: level select ignores confirmed during delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseLevelSelectTag>(reg);
    gs.phase_timer = 0.1f;  // within 0.2s delay
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});

    game_state_system(reg, 0.016f);

    // confirmed still latched, no transition yet
    CHECK_FALSE(is_phase_transition_pending(reg));
    CHECK(reg.ctx().contains<LevelSelectConfirmedTag>());
}

TEST_CASE("game_state: game_over restart button triggers playing", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.5f;
    reg.ctx().emplace<EndChoiceRestart>();

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
}

TEST_CASE("game_state: game_over main_menu button triggers title", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.5f;
    reg.ctx().emplace<EndChoiceMainMenu>();

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhaseTitleTag>());
}

TEST_CASE("game_state: game_over level_select button triggers level_select", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.5f;
    reg.ctx().emplace<EndChoiceLevelSelect>();

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhaseLevelSelectTag>());
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
}

TEST_CASE("game_state: game_over ignores choice at readiness boundary", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.4f;
    reg.ctx().emplace<EndChoiceRestart>();

    game_state_system(reg, 0.0f);

    CHECK_FALSE(is_phase_transition_pending(reg));
    // Tag persists across the input-delay window so the press is honored
    // once the delay elapses (pre-migration semantics).
    CHECK(reg.ctx().find<EndChoiceRestart>() != nullptr);
}

TEST_CASE("game_state: game_over ignores missing end choice", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.5f;
    REQUIRE(reg.ctx().find<EndChoiceRestart>() == nullptr);
    REQUIRE(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    REQUIRE(reg.ctx().find<EndChoiceMainMenu>() == nullptr);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
}

TEST_CASE("game_state: game_over restart enters fresh play session on next tick", "[gamestate][play_session]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.5f;
    reg.ctx().emplace<EndChoiceRestart>();

    auto stale = reg.create();
    reg.emplace<ObstacleTag>(stale);
    reg.emplace<WorldPosition>(stale, WorldPosition{{0.0f, 0.0f}});
    reg.ctx().get<ScoreState>().score = 3210;

    game_state_system(reg, 0.016f);
    REQUIRE(reg.ctx().contains<NextPhasePlayingTag>());

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
    CHECK(reg.ctx().contains<GamePhasePlayingTag>());
    CHECK_FALSE(reg.valid(stale));
    CHECK_FALSE(reg.view<PlayerTag>().empty());
    CHECK(reg.ctx().get<ScoreState>().score == 0);
}

TEST_CASE("game_state: title position tap triggers level_select", "[gamestate]") {
    auto reg = make_registry();
    set_test_phase<GamePhaseTitleTag>(reg);
    // Simulate a tap → Confirm menu button press (title screen has full-screen confirm)
    auto btn = make_menu_confirm_button(reg);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
    CHECK(reg.ctx().contains<GamePhaseLevelSelectTag>());
}

TEST_CASE("title settings navigation: pending Settings transition reaches Settings screen",
          "[gamestate][ui][settings][regression]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseTitleTag>(reg);
    gs.phase_timer = 2.0f;
    request_phase_transition<NextPhaseSettingsTag>(reg);

    // Proxy for title gear click path: the title-screen Settings tap
    // request enqueues a `NextPhaseSettingsTag` transition (seeded here
    // directly), and fixed-step consumes it.
    game_state_system(reg, 0.016f);
    REQUIRE(reg.ctx().contains<GamePhaseSettingsTag>());
    CHECK_FALSE(is_phase_transition_pending(reg));
    CHECK(gs.phase_timer == 0.0f);
}

TEST_CASE("game_state: transition_pending consumed on execution", "[gamestate]") {
    auto reg = make_registry();
    request_phase_transition<NextPhasePausedTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
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

    request_phase_transition<NextPhaseSongCompleteTag>(reg);

    game_state_system(reg, 0.016f);

    // score.score must NOT be zeroed — it is the final value rendered on the results screen
    CHECK(score.score == 12345);
    CHECK(reg.ctx().contains<GamePhaseSongCompleteTag>());
}
