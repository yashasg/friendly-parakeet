#include <catch2/catch_test_macros.hpp>
#include "systems/obstacle_despawn_system.h"
#include "test_helpers.h"

// ── motion_system ────────────────────────────────────────────
//
// Single path after #349 migration:
//   motion_view — WorldTransform+Vector2 (obstacles, popups, particles, player)
//                 also bridges to Position when both components are present.

TEST_CASE("motion: WorldTransform+Vector2 moves by velocity * dt", "[motion]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<Vector2>(e, Vector2{10.0f, 20.0f});

    motion_system(reg, 1.0f);

    CHECK(reg.get<WorldTransform>(e).position.x == 110.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == 220.0f);
}

TEST_CASE("motion: zero Vector2 means no movement", "[motion]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<Vector2>(e, Vector2{0.0f, 0.0f});

    motion_system(reg, 1.0f);

    CHECK(reg.get<WorldTransform>(e).position.x == 100.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == 200.0f);
}

// ── motion_system: WorldTransform+Vector2 path (modern, issue #349 target) ──

TEST_CASE("motion: WorldTransform+Vector2 entity moves by velocity * dt", "[motion]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<Vector2>(e, Vector2{10.0f, 20.0f});

    motion_system(reg, 1.0f);

    const auto& wt = reg.get<WorldTransform>(e);
    CHECK(wt.position.x == 110.0f);
    CHECK(wt.position.y == 220.0f);
}

TEST_CASE("motion: BeatInfo alone does not make an entity movable", "[motion]") {
    // Rhythm obstacles are song-time authoritative and should not carry
    // Vector2. BeatInfo by itself is ignored by motion_system.
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<BeatInfo>(e, BeatInfo{0, 0.0f, 0.0f});  // marks entity as beat-authoritative

    motion_system(reg, 1.0f);

    const auto& wt = reg.get<WorldTransform>(e);
    CHECK(wt.position.x == 100.0f);
    CHECK(wt.position.y == 200.0f);
}


// ── obstacle_despawn_system ───────────────────────────────────────────

TEST_CASE("cleanup: destroys obstacles past DESTROY_Y", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, constants::DESTROY_Y + 10.0f}});

    obstacle_despawn_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(obs));
}

TEST_CASE("cleanup: keeps obstacles above DESTROY_Y", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, constants::DESTROY_Y - 10.0f}});

    obstacle_despawn_system(reg, 0.016f);

    CHECK(reg.valid(obs));
}

TEST_CASE("cleanup: non-obstacle entities are untouched", "[cleanup]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{0.0f, constants::DESTROY_Y + 100.0f}});
    // No ObstacleTag

    obstacle_despawn_system(reg, 0.016f);

    CHECK(reg.valid(e));
}

// ── game_state_system ────────────────────────────────────────

TEST_CASE("game_state: title to level select on touch", "[gamestate]") {
    auto reg = make_registry();
    set_test_phase<GamePhaseTitleTag>(reg);
    auto btn = make_menu_button(reg, MenuActionKind::Confirm);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
    CHECK(reg.ctx().contains<GamePhaseLevelSelectTag>());
}

TEST_CASE("game_state: game over button choice after delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.5f;
    auto btn = make_menu_button(reg, MenuActionKind::GoLevelSelect);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    // Button tap sets choice AND transitions in same frame
    CHECK(reg.ctx().contains<NextPhaseLevelSelectTag>());
}

TEST_CASE("game_state: game over keyboard confirm routes to restart", "[gamestate][input][regression]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.5f;

    reg.ctx().get<entt::dispatcher>().enqueue<MenuPressEvent>(
        {MenuActionKind::Confirm, 0});

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
}

TEST_CASE("game_state: song complete keyboard confirm routes to restart", "[gamestate][input][regression]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseSongCompleteTag>(reg);
    gs.phase_timer = constants::SONG_COMPLETE_INPUT_DELAY + 0.1f;

    reg.ctx().get<entt::dispatcher>().enqueue<MenuPressEvent>(
        {MenuActionKind::Confirm, 0});

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<NextPhasePlayingTag>());
}

TEST_CASE("game_state: song complete keyboard confirm waits for button debounce",
          "[gamestate][input][regression]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseSongCompleteTag>(reg);
    gs.phase_timer = 0.45f;

    reg.ctx().get<entt::dispatcher>().enqueue<MenuPressEvent>(
        {MenuActionKind::Confirm, 0});

    game_state_system(reg, 0.0f);

    CHECK_FALSE(is_phase_transition_pending(reg));
    CHECK(reg.ctx().find<EndChoiceRestart>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceLevelSelect>() == nullptr);
    CHECK(reg.ctx().find<EndChoiceMainMenu>() == nullptr);
}

TEST_CASE("game_state: game over ignores touch during delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseGameOverTag>(reg);
    gs.phase_timer = 0.2f;  // within GAME_OVER_INPUT_DELAY
    auto btn = make_menu_button(reg, MenuActionKind::Confirm);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("game_state: paused menu input waits for entry debounce", "[gamestate][input]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhasePausedTag>(reg);
    gs.phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.1f;
    gs.phase_timer = 0.1f;

    reg.ctx().get<entt::dispatcher>().enqueue<MenuPressEvent>(
        {MenuActionKind::Confirm, 0});

    game_state_system(reg, 0.0f);

    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("game_state: paused menu input resumes after entry debounce", "[gamestate][input]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhasePausedTag>(reg);
    gs.phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.1f;

    reg.ctx().get<entt::dispatcher>().enqueue<MenuPressEvent>(
        {MenuActionKind::Confirm, 0});

    game_state_system(reg, 0.0f);

    CHECK(reg.ctx().contains<GamePhasePlayingTag>());
    CHECK(gs.phase_timer == 0.0f);
}

TEST_CASE("game_state: phase timer increments", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase_timer = 0.0f;

    game_state_system(reg, 0.5f);

    CHECK(reg.ctx().get<GameState>().phase_timer == 0.5f);
}

// ── game_state: transition execution ─────────────────────────

TEST_CASE("game_state: enter_playing clears entities and creates player", "[gamestate]") {
    auto reg = make_registry();
    // Create some entities that should be cleared
    auto junk = reg.create();
    reg.emplace<ObstacleTag>(junk);
    reg.emplace<WorldTransform>(junk, WorldTransform{{0.0f, 0.0f}});

    request_phase_transition<NextPhasePlayingTag>(reg);

    game_state_system(reg, 0.016f);

    // Original entity should be gone (registry cleared)
    CHECK_FALSE(reg.valid(junk));

    // A new player should exist
    auto player_view = reg.view<PlayerTag>();
    int player_count = 0;
    for (auto e : player_view) { ++player_count; (void)e; }
    CHECK(player_count == 1);

    // Phase should be Playing
    CHECK(reg.ctx().contains<GamePhasePlayingTag>());
}

TEST_CASE("game_state: enter_game_over updates high score", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 5000;
    current.value = 3000;

    request_phase_transition<NextPhaseGameOverTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(current.value == 5000);
    CHECK(reg.ctx().contains<GamePhaseGameOverTag>());
    const auto& terminal = reg.ctx().get<TerminalResultState>();
    CHECK(terminal.new_best);
    CHECK(terminal.previous_best == 3000);
}

TEST_CASE("game_state: enter_game_over pushes Crash SFX", "[gamestate]") {
    auto reg = make_registry();
    request_phase_transition<NextPhaseGameOverTag>(reg);

    game_state_system(reg, 0.016f);

    auto cap = drain_sfx_events(reg);
    bool found_crash = false;
    for (int i = 0; i < cap.count; ++i)
        if (cap.buf[i] == SFX::Crash) found_crash = true;
    CHECK(found_crash);
}

TEST_CASE("game_state: enter_game_over preserves high score if lower", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& current = reg.ctx().get<CurrentSongHighScore>();
    score.score = 1000;
    current.value = 5000;

    request_phase_transition<NextPhaseGameOverTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(current.value == 5000);
    const auto& terminal = reg.ctx().get<TerminalResultState>();
    CHECK_FALSE(terminal.new_best);
    CHECK(terminal.previous_best == 5000);
}

TEST_CASE("game_state: paused to playing on touch", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhasePausedTag>(reg);
    gs.phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.1f;
    auto btn = make_menu_button(reg, MenuActionKind::Confirm);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<GamePhasePlayingTag>());
    CHECK(gs.phase_timer == 0.0f);
}

TEST_CASE("game_state: paused resume preserves active play session state", "[gamestate][regression]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhasePausedTag>(reg);
    gs.phase_timer = constants::UI_ENTRY_DEBOUNCE + 0.1f;

    auto player = make_player(reg);
    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<WorldTransform>(obstacle, WorldTransform{{0.0f, 123.0f}});

    auto& score = reg.ctx().get<ScoreState>();
    score.score = 4321;

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.42f;
    energy.display = 0.37f;

    auto& song = reg.ctx().get<SongState>();
    song.song_time = 12.5f;
    song.playing = true;
    song.restart_music = false;

    auto btn = make_menu_button(reg, MenuActionKind::Confirm);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<GamePhasePlayingTag>());
    CHECK(gs.phase_timer == 0.0f);
    CHECK(reg.valid(player));
    CHECK(reg.valid(obstacle));
    CHECK(score.score == 4321);
    CHECK(energy.energy == 0.42f);
    CHECK(energy.display == 0.37f);
    CHECK(song.song_time == 12.5f);
    CHECK(song.playing);
    CHECK_FALSE(song.restart_music);

    int player_count = 0;
    for (auto e : reg.view<PlayerTag>()) {
        ++player_count;
        (void)e;
    }
    CHECK(player_count == 1);
}

TEST_CASE("game_state: title stays title without touch", "[gamestate]") {
    auto reg = make_registry();
    set_test_phase<GamePhaseTitleTag>(reg);
    // No press event in queue — no actions

    game_state_system(reg, 0.5f);

    CHECK(reg.ctx().contains<GamePhaseTitleTag>());
    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("game_state: transition to Paused sets phase", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhasePlayingTag>(reg);
    request_phase_transition<NextPhasePausedTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<GamePhasePausedTag>());
    CHECK(gs.phase_timer == 0.0f);
    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("game_state: transition to Settings sets phase", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    set_test_phase<GamePhaseTitleTag>(reg);
    request_phase_transition<NextPhaseSettingsTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().contains<GamePhaseSettingsTag>());
    CHECK(gs.phase_timer == 0.0f);
    CHECK_FALSE(is_phase_transition_pending(reg));
}

TEST_CASE("game_state: enter_playing resets score", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().get<ScoreState>().score = 9999;

    request_phase_transition<NextPhasePlayingTag>(reg);

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().get<ScoreState>().score == 0);
}

// ── scroll_system: phase guard ──────────────────────────────

TEST_CASE("scroll: no movement when not in Playing phase", "[scroll]") {
    auto reg = make_registry();
    set_test_phase<GamePhaseTitleTag>(reg);
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<Vector2>(e, Vector2{10.0f, 20.0f});

    scroll_system(reg, 1.0f);

    CHECK(reg.get<WorldTransform>(e).position.x == 100.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == 200.0f);
}

TEST_CASE("motion: no movement when not in Playing phase", "[motion]") {
    auto reg = make_registry();
    set_test_phase<GamePhaseTitleTag>(reg);
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<Vector2>(e, Vector2{10.0f, 20.0f});

    tick_playing_systems(reg, 1.0f);

    CHECK(reg.get<WorldTransform>(e).position.x == 100.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == 200.0f);
}

// ── cleanup: edge cases ─────────────────────────────────────

TEST_CASE("cleanup: obstacle at exactly DESTROY_Y is kept", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, constants::DESTROY_Y}});

    obstacle_despawn_system(reg, 0.016f);

    CHECK(reg.valid(obs));
}

// #242 — static buffer must handle multiple entities in one pass.
TEST_CASE("cleanup: destroys multiple obstacles past DESTROY_Y in one pass", "[cleanup]") {
    auto reg = make_registry();

    constexpr int N = 5;
    entt::entity obs[N];
    for (int i = 0; i < N; ++i) {
        obs[i] = reg.create();
        reg.emplace<ObstacleTag>(obs[i]);
        reg.emplace<WorldTransform>(obs[i], WorldTransform{{0.0f, constants::DESTROY_Y + static_cast<float>(i + 1) * 10.0f}});
    }

    obstacle_despawn_system(reg, 0.016f);

    for (int i = 0; i < N; ++i)
        CHECK_FALSE(reg.valid(obs[i]));
}

// #1089 — ObstacleDespawnScratch::capacity_exceeded_count must stay at zero
// when a dense pass fits inside the reserved to_destroy buffer.
TEST_CASE("cleanup: dense despawn burst stays within reserved capacity",
          "[cleanup][issue1089]") {
    auto reg = make_registry();
    constexpr int dense_count = 6;
    runtime_system_scratch_reserve(reg, dense_count);

    auto& scratch = reg.ctx().get<ObstacleDespawnScratch>();
    const auto destroy_capacity = scratch.to_destroy.capacity();

    for (int i = 0; i < dense_count; ++i) {
        auto obs = reg.create();
        reg.emplace<ObstacleTag>(obs);
        reg.emplace<WorldTransform>(obs,
            WorldTransform{{0.0f, constants::DESTROY_Y + static_cast<float>(i + 1) * 10.0f}});
    }

    obstacle_despawn_system(reg, 0.016f);

    CHECK(scratch.to_destroy.capacity() == destroy_capacity);
    CHECK(scratch.capacity_exceeded_count == 0u);
}

// #242 / #280 — cleanup must not emplace MissTag or ScoredTag; that is miss_detection_system's job.
TEST_CASE("cleanup: does not emplace MissTag or ScoredTag on surviving obstacles", "[cleanup]") {
    auto reg = make_registry();

    auto survivor = reg.create();
    reg.emplace<ObstacleTag>(survivor);
    reg.emplace<WorldTransform>(survivor, WorldTransform{{0.0f, constants::DESTROY_Y - 1.0f}});

    obstacle_despawn_system(reg, 0.016f);

    CHECK(reg.valid(survivor));
    CHECK_FALSE(reg.all_of<MissTag>(survivor));
    CHECK_FALSE(reg.all_of<ScoredTag>(survivor));
}
