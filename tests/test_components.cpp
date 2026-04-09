#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

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

TEST_CASE("components: BurnoutState defaults to no threat", "[components]") {
    BurnoutState bs{};
    CHECK(bs.meter == 0.0f);
    CHECK(bs.zone == BurnoutZone::None);
    CHECK((bs.nearest_threat == entt::null));
}

TEST_CASE("components: DifficultyConfig defaults", "[components]") {
    DifficultyConfig dc{};
    CHECK(dc.speed_multiplier == 1.0f);
    CHECK(dc.elapsed == 0.0f);
}

TEST_CASE("components: GameState defaults to title", "[components]") {
    GameState gs{};
    CHECK(gs.phase == GamePhase::Title);
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("ecs: make_registry creates all singletons", "[ecs]") {
    auto reg = make_registry();
    // These should not throw
    static_cast<void>(reg.ctx().get<InputState>());
    static_cast<void>(reg.ctx().get<GestureResult>());
    static_cast<void>(reg.ctx().get<ShapeButtonEvent>());
    static_cast<void>(reg.ctx().get<GameState>());
    static_cast<void>(reg.ctx().get<ScoreState>());
    static_cast<void>(reg.ctx().get<BurnoutState>());
    static_cast<void>(reg.ctx().get<DifficultyConfig>());
    static_cast<void>(reg.ctx().get<AudioQueue>());
    SUCCEED("all required singletons exist in registry context");
}

TEST_CASE("ecs: make_player creates proper entity", "[ecs]") {
    auto reg = make_registry();
    auto p = make_player(reg);

    CHECK(reg.all_of<PlayerTag>(p));
    CHECK(reg.all_of<Position>(p));
    CHECK(reg.all_of<PlayerShape>(p));
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

TEST_CASE("components: GestureResult default is None", "[components]") {
    GestureResult g{};
    CHECK(g.gesture == Gesture::None);
    CHECK(g.magnitude == 0.0f);
    CHECK(g.hit_x == 0.0f);
    CHECK(g.hit_y == 0.0f);
}

TEST_CASE("components: Lifetime defaults", "[components]") {
    Lifetime lt{};
    CHECK(lt.remaining == 0.0f);
    CHECK(lt.max_time == 0.0f);
}

TEST_CASE("components: Color construction", "[components]") {
    Color c{uint8_t{255}, uint8_t{128}, uint8_t{64}, uint8_t{200}};
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
    ParticleData pd{10.0f, 10.0f};
    CHECK(pd.size == 10.0f);
    CHECK(pd.start_size == 10.0f);
}

TEST_CASE("components: ShapeButtonEvent defaults to not pressed", "[components]") {
    ShapeButtonEvent sbe{};
    CHECK_FALSE(sbe.pressed);
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
