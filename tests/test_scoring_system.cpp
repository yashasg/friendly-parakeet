#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE("scoring: distance bonus accumulates", "[scoring]") {
    auto reg = make_registry();

    scoring_system(reg, 1.0f);

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.score >= constants::PTS_PER_SECOND);
    CHECK(score.distance_traveled > 0.0f);
}

TEST_CASE("scoring: scored obstacle awards points", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.score >= constants::PTS_SHAPE_GATE);
}

TEST_CASE("scoring: chain bonus increases points", "[scoring]") {
    auto reg = make_registry();

    // Score 3 obstacles in a row
    for (int i = 0; i < 3; ++i) {
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y + float(i));
        reg.emplace<ScoredTag>(obs);
        scoring_system(reg, 0.016f);
    }

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.chain_count == 3);
    // Total should be more than 3x base due to chain bonuses
    int base_only = 3 * constants::PTS_SHAPE_GATE;
    CHECK(score.score > base_only);
}

TEST_CASE("scoring: chain resets after timeout", "[scoring]") {
    auto reg = make_registry();

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    scoring_system(reg, 0.016f);
    CHECK(reg.ctx().get<ScoreState>().chain_count == 1);

    // Wait > 2 seconds
    scoring_system(reg, 2.5f);

    CHECK(reg.ctx().get<ScoreState>().chain_count == 0);
}

TEST_CASE("scoring: popup entity spawned on score", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    int popup_count = 0;
    for (auto e : popup_view) {
        CHECK(reg.all_of<WorldTransform>(e));
        CHECK(reg.all_of<MotionVelocity>(e));
        CHECK(reg.all_of<TagHUDPass>(e));
        CHECK_FALSE(reg.all_of<Position>(e));
        CHECK_FALSE(reg.all_of<Velocity>(e));
        ++popup_count;
    }
    CHECK(popup_count == 1);
}

TEST_CASE("scoring: SFX pushed on score", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    CHECK(reg.ctx().get<AudioQueue>().count > 0);
}

TEST_CASE("scoring: displayed_score rolls up toward score", "[scoring]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 1000;
    score.displayed_score = 0;

    scoring_system(reg, 0.1f);

    CHECK(score.displayed_score > 0);
    CHECK(score.displayed_score <= score.score);
}

TEST_CASE("scoring: not in Playing phase skips processing", "[scoring]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;

    scoring_system(reg, 1.0f);

    CHECK(reg.ctx().get<ScoreState>().score == 0);
}

TEST_CASE("scoring: chain bonus 5+ gives extended bonus", "[scoring]") {
    auto reg = make_registry();

    // Score 5 obstacles in a row (chain_count 1..5)
    for (int i = 0; i < 5; ++i) {
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y + float(i));
        reg.emplace<ScoredTag>(obs);
        scoring_system(reg, 0.016f);
    }

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.chain_count == 5);
    // 5th obstacle: base + CHAIN_BONUS[4] + (5-4)*100 = 200 + 200 + 100 = 500
    // Total for 5 obstacles should be significantly more than 5*200
    CHECK(score.score > 5 * constants::PTS_SHAPE_GATE);
}

TEST_CASE("scoring: obstacle entity cleaned up after scoring", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    // Obstacle and ScoredTag components should be removed
    CHECK_FALSE(reg.all_of<Obstacle>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
    // Entity itself still exists (for scroll/cleanup)
    CHECK(reg.valid(obs));
}

TEST_CASE("scoring: displayed_score does not overshoot score", "[scoring]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 100;
    score.displayed_score = 99;

    // Very large dt that would normally overshoot
    scoring_system(reg, 10.0f);

    // displayed_score should not exceed score (capped)
    // Note: score increases by dt * PTS_PER_SECOND too
    CHECK(score.displayed_score <= score.score);
}

TEST_CASE("scoring: distance_traveled accumulates from scroll speed", "[scoring]") {
    auto reg = make_registry();
    auto& song = reg.ctx().get<SongState>();
    song.scroll_speed = 400.0f;

    scoring_system(reg, 1.0f);

    CHECK(reg.ctx().get<ScoreState>().distance_traveled == 400.0f);
}

// On-beat shape gate scores at base points (timing only, no burnout multiplier).
TEST_CASE("scoring: no-penalty — on-beat gate scores at base points", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.0f);  // dt=0 excludes distance bonus for exact assertion

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.score == constants::PTS_SHAPE_GATE);
}
