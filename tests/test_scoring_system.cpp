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

    // Set burnout zone to Safe (x1.0 multiplier)
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.score >= constants::PTS_SHAPE_GATE);
}

TEST_CASE("scoring: burnout multiplier affects points", "[scoring]") {
    // Run at Safe (x1) and Danger (x3) and compare
    auto reg1 = make_registry();
    auto obs1 = make_shape_gate(reg1, Shape::Circle, constants::PLAYER_Y);
    reg1.emplace<ScoredTag>(obs1);
    reg1.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;
    scoring_system(reg1, 0.016f);
    int safe_score = reg1.ctx().get<ScoreState>().score;

    auto reg2 = make_registry();
    auto obs2 = make_shape_gate(reg2, Shape::Circle, constants::PLAYER_Y);
    reg2.emplace<ScoredTag>(obs2);
    reg2.ctx().get<BurnoutState>().zone = BurnoutZone::Danger;
    scoring_system(reg2, 0.016f);
    int danger_score = reg2.ctx().get<ScoreState>().score;

    CHECK(danger_score > safe_score);
}

TEST_CASE("scoring: chain bonus increases points", "[scoring]") {
    auto reg = make_registry();
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;

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
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;

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
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;

    scoring_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    int popup_count = 0;
    for (auto e : popup_view) { ++popup_count; (void)e; }
    CHECK(popup_count == 1);
}

TEST_CASE("scoring: BurnoutBank SFX pushed on score", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;

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

TEST_CASE("scoring: clutch/dead zone multiplier applies 5x", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Dead;

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // Base 200 * 5.0 = 1000, plus distance bonus
    CHECK(score.score >= 1000);
}

TEST_CASE("scoring: chain bonus 5+ gives extended bonus", "[scoring]") {
    auto reg = make_registry();
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;

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
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;

    scoring_system(reg, 0.016f);

    // Obstacle and ScoredTag components should be removed
    CHECK_FALSE(reg.all_of<Obstacle>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
    // Entity itself still exists (for scroll/cleanup)
    CHECK(reg.valid(obs));
}

TEST_CASE("scoring: score popup has correct tier for multiplier", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Danger;  // 3.0x mult

    scoring_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    for (auto [e, popup] : popup_view.each()) {
        CHECK(popup.tier == 3);  // tier_for_multiplier(3.0) = 3
    }
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

TEST_CASE("scoring: risky multiplier applies 1.5x", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Risky;

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // Base 200 * 1.5 = 300, plus distance bonus
    CHECK(score.score >= 300);
}

TEST_CASE("scoring: distance_traveled accumulates from scroll speed", "[scoring]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.scroll_speed = 400.0f;

    scoring_system(reg, 1.0f);

    CHECK(reg.ctx().get<ScoreState>().distance_traveled == 400.0f);
}
