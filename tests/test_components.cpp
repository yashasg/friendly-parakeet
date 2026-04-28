#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "audio/audio_queue.h"
#include "input/input_state.h"
#include "input/phase_activation.h"

// Verify component defaults and basic ECS operations

TEST_CASE("components: Position default is zero", "[components]") {
    Position p{};
    CHECK(p.x == 0.0f);
    CHECK(p.y == 0.0f);
}

TEST_CASE("components: PlayerShape defaults to Circle", "[components]") {
    PlayerShape ps{};
    CHECK(ps.current == Shape::Circle);
    CHECK(ps.previous == Shape::Circle);
    CHECK(ps.morph_t == 1.0f);
}

TEST_CASE("components: Lane defaults to center", "[components]") {
    Lane l{};
    CHECK(l.current == 1);
    CHECK(l.target == -1);
    CHECK(l.lerp_t == 1.0f);
}

TEST_CASE("components: VerticalState defaults to grounded", "[components]") {
    VerticalState vs{};
    CHECK(vs.mode == VMode::Grounded);
    CHECK(vs.timer == 0.0f);
    CHECK(vs.y_offset == 0.0f);
}

TEST_CASE("components: InputState clear_events resets flags", "[components]") {
    InputState is{};
    is.touch_down = true;
    is.touch_up = true;
    clear_input_events(is);
    CHECK_FALSE(is.touch_down);
    CHECK_FALSE(is.touch_up);
}

TEST_CASE("components: AudioQueue push and clear", "[components]") {
    AudioQueue q{};
    CHECK(q.count == 0);
    audio_push(q, SFX::ShapeShift);
    audio_push(q, SFX::Crash);
    CHECK(q.count == 2);
    CHECK(q.queue[0] == SFX::ShapeShift);
    CHECK(q.queue[1] == SFX::Crash);
    audio_clear(q);
    CHECK(q.count == 0);
}

TEST_CASE("components: AudioQueue overflow protection", "[components]") {
    AudioQueue q{};
    for (int i = 0; i < AudioQueue::MAX_QUEUED + 5; ++i) {
        audio_push(q, SFX::UITap);
    }
    CHECK(q.count == AudioQueue::MAX_QUEUED);
}

TEST_CASE("components: ScoreState defaults to zero", "[components]") {
    ScoreState s{};
    CHECK(s.score == 0);
    CHECK(s.high_score == 0);
    CHECK(s.chain_count == 0);
}

TEST_CASE("components: GameState defaults to title", "[components]") {
    GameState gs{};
    CHECK(gs.phase == GamePhase::Title);
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("ecs: make_registry creates all singletons", "[ecs]") {
    auto reg = make_registry();
    // These should not throw — assert every ctx singleton required by systems.
    // Input / event transport
    static_cast<void>(reg.ctx().get<InputState>());
    static_cast<void>(reg.ctx().get<entt::dispatcher>());   // #332: must be present
    // Gameplay state
    static_cast<void>(reg.ctx().get<GameState>());
    static_cast<void>(reg.ctx().get<ScoreState>());
    // Audio / haptics / settings
    static_cast<void>(reg.ctx().get<AudioQueue>());
    static_cast<void>(reg.ctx().get<HapticQueue>());
    static_cast<void>(reg.ctx().get<SettingsState>());
    // Level / song / rhythm
    static_cast<void>(reg.ctx().get<LevelSelectState>());
    static_cast<void>(reg.ctx().get<BeatMap>());
    static_cast<void>(reg.ctx().get<SongState>());
    static_cast<void>(reg.ctx().get<EnergyState>());
    static_cast<void>(reg.ctx().get<SongResults>());
    // Scoring persistence / end-screen
    static_cast<void>(reg.ctx().get<HighScoreState>());
    static_cast<void>(reg.ctx().get<HighScorePersistence>());
    static_cast<void>(reg.ctx().get<GameOverState>());
    // Misc
    static_cast<void>(reg.ctx().get<RNGState>());
    static_cast<void>(reg.ctx().get<ObstacleCounter>());
    SUCCEED("all required ctx singletons exist in registry context");
}

TEST_CASE("ecs: make_registry dispatcher is wired — GoEvent listeners registered", "[ecs][dispatcher]") {
    // Verifies that wire_input_dispatcher() was called during make_registry(),
    // so any system can immediately enqueue+update GoEvents without separate setup.
    auto reg = make_registry();

    // Promote to Playing so player_input_handle_go has observable effect.
    reg.ctx().get<GameState>().phase = GamePhase::Playing;
    auto player = make_player(reg);
    auto& lane  = reg.get<Lane>(player);

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.enqueue(GoEvent{Direction::Right});
    disp.update<GoEvent>();

    // If dispatcher listeners were NOT wired, lane.target would remain -1.
    CHECK(lane.target == 2);   // listener wired: player_input_handle_go fired
}

TEST_CASE("ecs: make_registry dispatcher is wired — ButtonPressEvent listeners registered", "[ecs][dispatcher]") {
    // Verifies ButtonPressEvent sink is wired: a press event on a valid button
    // in Playing phase reaches player_input_handle_press.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& sw    = reg.get<ShapeWindow>(player);
    REQUIRE(sw.phase == WindowPhase::Idle);

    auto btn = make_shape_button(reg, Shape::Triangle);
    reg.get<Position>(btn) = {0.f, 0.f};
    reg.get<HitCircle>(btn).radius = 50.f;

    auto& disp = reg.ctx().get<entt::dispatcher>();
    press_button(reg, btn);
    disp.update<ButtonPressEvent>();

    // If dispatcher listeners were NOT wired, sw.phase would stay Idle.
    CHECK(sw.phase        == WindowPhase::MorphIn);  // listener wired: handle_press fired
    CHECK(sw.target_shape == Shape::Triangle);
}

TEST_CASE("ecs: make_registry dispatcher ctx — second update is a no-op (no replay)", "[ecs][dispatcher]") {
    // Contract: after an authoritative drain, a subsequent update<T>() with no
    // new enqueues must not re-deliver the previously drained event.
    // Applies to both GoEvent and ButtonPressEvent pools.
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& lane  = reg.get<Lane>(player);

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.enqueue(GoEvent{Direction::Right});
    disp.update<GoEvent>();
    CHECK(lane.target == 2);

    // Second drain — must be a no-op.
    disp.update<GoEvent>();
    CHECK(lane.target == 2);   // not replayed
}

TEST_CASE("ecs: wire_input_dispatcher is idempotent", "[ecs][dispatcher]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto btn = make_shape_button(reg, Shape::Triangle);
    reg.get<Position>(btn) = {0.f, 0.f};
    reg.get<HitCircle>(btn).radius = 50.f;

    wire_input_dispatcher(reg);
    press_button(reg, btn);
    reg.ctx().get<entt::dispatcher>().update<ButtonPressEvent>();

    CHECK(reg.get<ShapeWindow>(player).phase == WindowPhase::MorphIn);
    CHECK(reg.ctx().get<AudioQueue>().count == 1);
}

struct ExternalGoListener {
    int count = 0;
    void on_go(const GoEvent&) { ++count; }
};

TEST_CASE("ecs: unwire_input_dispatcher preserves external listeners", "[ecs][dispatcher]") {
    auto reg = make_registry();
    auto& disp = reg.ctx().get<entt::dispatcher>();
    ExternalGoListener listener;
    disp.sink<GoEvent>().connect<&ExternalGoListener::on_go>(listener);

    unwire_input_dispatcher(reg);
    disp.trigger(GoEvent{Direction::Left});

    CHECK(listener.count == 1);
}

TEST_CASE("ecs: make_player creates proper entity", "[ecs]") {
    auto reg = make_registry();
    auto p = make_player(reg);

    CHECK(reg.all_of<PlayerTag>(p));
    CHECK(reg.all_of<Position>(p));
    CHECK(reg.all_of<PlayerShape>(p));
    CHECK(reg.all_of<ShapeWindow>(p));
    CHECK(reg.all_of<Lane>(p));
    CHECK(reg.all_of<VerticalState>(p));
    CHECK(reg.all_of<Color>(p));
    CHECK(reg.all_of<DrawSize>(p));
    CHECK(reg.all_of<DrawLayer>(p));
}

TEST_CASE("components: Velocity default is zero", "[components]") {
    Velocity v{};
    CHECK(v.dx == 0.0f);
    CHECK(v.dy == 0.0f);
}

TEST_CASE("components: Color construction", "[components]") {
    Color c{255, 128, 64, 200};
    CHECK(c.r == 255);
    CHECK(c.g == 128);
    CHECK(c.b == 64);
    CHECK(c.a == 200);
}

TEST_CASE("components: DrawSize construction", "[components]") {
    DrawSize ds{100.0f, 50.0f};
    CHECK(ds.w == 100.0f);
    CHECK(ds.h == 50.0f);
}

TEST_CASE("components: Obstacle construction", "[components]") {
    Obstacle obs{ObstacleKind::ShapeGate, int16_t{200}};
    CHECK(obs.kind == ObstacleKind::ShapeGate);
    CHECK(obs.base_points == 200);
}

TEST_CASE("components: ParticleData construction", "[components]") {
    ParticleData pd{10.0f};
    CHECK(pd.size == 10.0f);
}

TEST_CASE("ecs: make_combo_gate creates proper entity", "[ecs]") {
    auto reg = make_registry();
    auto obs = make_combo_gate(reg, Shape::Circle, 0b101, 500.0f);

    CHECK(reg.all_of<ObstacleTag>(obs));
    CHECK(reg.all_of<Position>(obs));
    CHECK(reg.all_of<Obstacle>(obs));
    CHECK(reg.all_of<RequiredShape>(obs));
    CHECK(reg.all_of<BlockedLanes>(obs));
    CHECK(reg.get<Obstacle>(obs).kind == ObstacleKind::ComboGate);
    CHECK(reg.get<RequiredShape>(obs).shape == Shape::Circle);
    CHECK(reg.get<BlockedLanes>(obs).mask == 0b101);
}

TEST_CASE("ecs: make_split_path creates proper entity", "[ecs]") {
    auto reg = make_registry();
    auto obs = make_split_path(reg, Shape::Triangle, 2, 500.0f);

    CHECK(reg.all_of<ObstacleTag>(obs));
    CHECK(reg.all_of<Obstacle>(obs));
    CHECK(reg.all_of<RequiredShape>(obs));
    CHECK(reg.all_of<RequiredLane>(obs));
    CHECK(reg.get<Obstacle>(obs).kind == ObstacleKind::SplitPath);
    CHECK(reg.get<RequiredShape>(obs).shape == Shape::Triangle);
    CHECK(reg.get<RequiredLane>(obs).lane == 2);
}

// ── GamePhaseBit / ActiveInPhase typed mask tests ─────────────────────────

TEST_CASE("GamePhaseBit: single-phase mask matches only that phase", "[phase_mask]") {
    ActiveInPhase aip{ GamePhaseBit::Playing };
    CHECK( phase_active(aip, GamePhase::Playing));
    CHECK(!phase_active(aip, GamePhase::Title));
    CHECK(!phase_active(aip, GamePhase::LevelSelect));
    CHECK(!phase_active(aip, GamePhase::Paused));
    CHECK(!phase_active(aip, GamePhase::GameOver));
    CHECK(!phase_active(aip, GamePhase::SongComplete));
}

TEST_CASE("GamePhaseBit: multi-phase OR mask covers both phases", "[phase_mask]") {
    ActiveInPhase aip{ GamePhaseBit::GameOver | GamePhaseBit::SongComplete };
    CHECK( phase_active(aip, GamePhase::GameOver));
    CHECK( phase_active(aip, GamePhase::SongComplete));
    CHECK(!phase_active(aip, GamePhase::Playing));
    CHECK(!phase_active(aip, GamePhase::Title));
    CHECK(!phase_active(aip, GamePhase::LevelSelect));
    CHECK(!phase_active(aip, GamePhase::Paused));
}

TEST_CASE("GamePhaseBit: all six phases have distinct bits", "[phase_mask]") {
    // Each GamePhaseBit value must be a distinct power-of-two
    CHECK(GamePhaseBit::Title        != GamePhaseBit::LevelSelect);
    CHECK(GamePhaseBit::LevelSelect  != GamePhaseBit::Playing);
    CHECK(GamePhaseBit::Playing      != GamePhaseBit::Paused);
    CHECK(GamePhaseBit::Paused       != GamePhaseBit::GameOver);
    CHECK(GamePhaseBit::GameOver     != GamePhaseBit::SongComplete);
    // Combined mask must not equal any single bit
    GamePhaseBit combined = GamePhaseBit::GameOver | GamePhaseBit::SongComplete;
    CHECK(combined != GamePhaseBit::GameOver);
    CHECK(combined != GamePhaseBit::SongComplete);
}

TEST_CASE("GamePhaseBit: to_phase_bit round-trips all GamePhase values", "[phase_mask]") {
    using P = GamePhase;
    using B = GamePhaseBit;
    CHECK(to_phase_bit(P::Title)        == B::Title);
    CHECK(to_phase_bit(P::LevelSelect)  == B::LevelSelect);
    CHECK(to_phase_bit(P::Playing)      == B::Playing);
    CHECK(to_phase_bit(P::Paused)       == B::Paused);
    CHECK(to_phase_bit(P::GameOver)     == B::GameOver);
    CHECK(to_phase_bit(P::SongComplete) == B::SongComplete);
}

TEST_CASE("GamePhaseBit: empty mask is inactive for all phases", "[phase_mask]") {
    ActiveInPhase aip{};  // default-constructed GamePhaseBit
    CHECK(!phase_active(aip, GamePhase::Title));
    CHECK(!phase_active(aip, GamePhase::Playing));
    CHECK(!phase_active(aip, GamePhase::GameOver));
}
