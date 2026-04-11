#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "components/test_player.h"
#include "session_logger.h"

#ifdef PLATFORM_DESKTOP

static entt::registry make_test_player_registry(TestPlayerSkill skill = TestPlayerSkill::Pro) {
    entt::registry reg = make_rhythm_registry();
    auto& tp = reg.ctx().emplace<TestPlayerState>();
    tp.skill = skill;
    tp.active = true;
    tp.rng.seed(42);
    return reg;
}

static entt::entity make_shape_gate_at_lane(entt::registry& reg, Shape shape, int8_t lane, float y) {
    auto obs = make_shape_gate(reg, shape, y);
    reg.get<Position>(obs).x = constants::LANE_X[lane];
    auto& song = reg.ctx().get<SongState>();
    float arrival = song.song_time + (constants::PLAYER_Y - y) / song.scroll_speed;
    reg.emplace<BeatInfo>(obs, 0, arrival, song.song_time);
    return obs;
}

static entt::entity make_lane_block_at(entt::registry& reg, uint8_t blocked_mask, float y) {
    auto obs = make_lane_block(reg, blocked_mask, y);
    auto& song = reg.ctx().get<SongState>();
    float arrival = song.song_time + (constants::PLAYER_Y - y) / song.scroll_speed;
    reg.emplace<BeatInfo>(obs, 0, arrival, song.song_time);
    return obs;
}

static void tick_systems(entt::registry& reg, int frames, float dt = 1.0f / 60.0f) {
    for (int i = 0; i < frames; ++i) {
        auto* song = reg.ctx().find<SongState>();
        if (song && song->playing) song->song_time += dt;

        test_player_system(reg, dt);
        gesture_system(reg, dt);
        player_action_system(reg, dt);
        shape_window_system(reg, dt);
        player_movement_system(reg, dt);
        scroll_system(reg, dt);
        collision_system(reg, dt);
        scoring_system(reg, dt);
        cleanup_system(reg, dt);
        auto& input = reg.ctx().get<InputState>();
        clear_input_events(input);

        // Stop early if game over
        if (reg.ctx().get<GameState>().transition_pending) break;
    }
}

static bool survived(entt::registry& reg) {
    auto& gs = reg.ctx().get<GameState>();
    return gs.phase == GamePhase::Playing && !gs.transition_pending;
}

// ── CORE: shape gate in same lane ────────────────────────────

TEST_CASE("test_player: clears shape gate in same lane", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    make_shape_gate_at_lane(reg, Shape::Circle, 1, constants::PLAYER_Y - 400.0f);
    tick_systems(reg, 300);
    CHECK(survived(reg));
}

// ── CORE: shape gate requiring lane change ───────────────────

TEST_CASE("test_player: clears shape gate requiring lane change", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    make_shape_gate_at_lane(reg, Shape::Triangle, 2, constants::PLAYER_Y - 600.0f);
    tick_systems(reg, 300);
    CHECK(survived(reg));
}

// ── CORE: lane block dodge ───────────────────────────────────

TEST_CASE("test_player: clears lane block", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    make_lane_block_at(reg, 0b011, constants::PLAYER_Y - 500.0f);
    tick_systems(reg, 300);
    CHECK(survived(reg));
}

// ── KEY FIX: shape+lane on same obstacle is not self-blocked ──

TEST_CASE("test_player: shape+lane action is not blocked by own shape press", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    make_shape_gate_at_lane(reg, Shape::Circle, 0, constants::PLAYER_Y - 600.0f);
    tick_systems(reg, 300);
    CHECK(survived(reg));
}

// ── KEY FIX: shape gate then lane block in sequence ──────────

TEST_CASE("test_player: shape gate then lane block sequential", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    // Space obstacles far enough apart that shape gate resolves well before
    // lane block arrives. At scroll=520px/s, 400px gap ≈ 0.77s.
    make_shape_gate_at_lane(reg, Shape::Square, 1, constants::PLAYER_Y - 300.0f);
    make_lane_block_at(reg, 0b010, constants::PLAYER_Y - 900.0f);
    tick_systems(reg, 600);
    CHECK(survived(reg));
}

// ── KEY FIX: lane change blocked while earlier shape resolves ──

TEST_CASE("test_player: lane dodge waits for earlier shape gate", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    // Gate at 300px above player, block at 1000px above (huge gap)
    make_shape_gate_at_lane(reg, Shape::Square, 1, constants::PLAYER_Y - 300.0f);
    make_lane_block_at(reg, 0b010, constants::PLAYER_Y - 1000.0f);
    tick_systems(reg, 800);
    CHECK(survived(reg));
}

// ── KEY FIX: effective lane accounts for pending actions ─────

TEST_CASE("test_player: plans lane dodge using effective future lane", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    make_shape_gate_at_lane(reg, Shape::Circle, 0, constants::PLAYER_Y - 300.0f);
    make_lane_block_at(reg, 0b001, constants::PLAYER_Y - 900.0f);
    tick_systems(reg, 600);
    CHECK(survived(reg));
}

// ── AUTO-START ───────────────────────────────────────────────

TEST_CASE("test_player: auto-starts from title screen", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    gs.phase_timer = 1.0f;
    test_player_system(reg, 0.016f);
    CHECK(reg.ctx().get<InputState>().touch_up);
}

// ── SWIPE COOLDOWN ───────────────────────────────────────────

TEST_CASE("test_player: swipe cooldown blocks immediate second swipe", "[test_player]") {
    auto reg = make_test_player_registry();
    make_rhythm_player(reg);
    auto& tp = reg.ctx().get<TestPlayerState>();
    tp.swipe_cooldown_timer = 0.125f;

    auto obs = make_lane_block_at(reg, 0b010, constants::PLAYER_Y - 300.0f);
    TestPlayerAction action;
    action.obstacle = obs;
    action.timer = -1.0f;
    action.target_lane = 0;
    action.arrival_time = reg.ctx().get<SongState>().song_time + 1.0f;
    tp.push_action(action);
    tp.mark_planned(obs);

    auto& input = reg.ctx().get<InputState>();
    test_player_system(reg, 0.016f);
    CHECK_FALSE(input.key_a);
    CHECK_FALSE(input.key_d);
}

#endif
