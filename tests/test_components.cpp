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
