#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── scoring_system: timing multiplier integration ────────────

TEST_CASE("scoring: perfect timing multiplier gives 1.5x base", "[scoring][rhythm]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Perfect, 1.0f);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // 200 * 1.5 * 1.0 = 300, plus distance bonus and chain
    CHECK(score.score >= 300);
}

TEST_CASE("scoring: bad timing multiplier gives 0.25x base", "[scoring][rhythm]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Bad, 0.0f);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // 200 * 0.25 * 1.0 = 50, plus distance bonus
    // Should be less than a normal hit
    CHECK(score.score < constants::PTS_SHAPE_GATE + 50);
}

TEST_CASE("scoring: perfect timing and base points combine correctly", "[scoring][rhythm]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Perfect, 1.0f);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // 200 * 1.5 (perfect) = 300, plus distance bonus
    CHECK(score.score >= 300);
}

TEST_CASE("scoring: SongResults tracks max_chain", "[scoring]") {
    auto reg = make_registry();

    for (int i = 0; i < 4; ++i) {
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y + float(i));
        reg.emplace<ScoredTag>(obs);
        scoring_system(reg, 0.016f);
    }

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.max_chain == 4);
}

TEST_CASE("scoring: TimingGrade removed from entity after scoring", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<TimingGrade>(obs));
}

TEST_CASE("scoring: popup timing_tier set for graded obstacles", "[scoring][rhythm]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    for (auto [e, popup] : popup_view.each()) {
        CHECK(popup.timing_tier == TimingTier::Good);
    }
}

TEST_CASE("scoring: popup timing_tier is nullopt for ungraded obstacles", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    // No TimingGrade

    scoring_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    for (auto [e, popup] : popup_view.each()) {
        CHECK(!popup.timing_tier.has_value());
    }
}

TEST_CASE("scoring: multiple obstacles scored in single frame", "[scoring]") {
    auto reg = make_registry();

    auto obs1 = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs1);
    auto obs2 = make_lane_block(reg, 0b001, constants::PLAYER_Y + 1.0f);
    reg.emplace<ScoredTag>(obs2);

    scoring_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    int popup_count = 0;
    for (auto e : popup_view) { ++popup_count; (void)e; }
    CHECK(popup_count == 2);
}

TEST_CASE("scoring: miss-tagged obstacles do not award score and reset chain", "[scoring]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.chain_count = 3;
    score.chain_timer = 0.5f;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<MissTag>(obs);

    int score_before = score.score;
    scoring_system(reg, 0.016f);

    CHECK(score.chain_count == 0);
    CHECK(score.chain_timer == 0.0f);
    CHECK(score.score == score_before + static_cast<int>(0.016f * constants::PTS_PER_SECOND));
    CHECK_FALSE(reg.all_of<Obstacle>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}

// ── LanePush exclusion from chain and popup ──────────────────────────────────

TEST_CASE("scoring: LanePush excluded from chain and popup", "[scoring][lane_push]") {
    auto reg = make_registry();
    make_player(reg);

    auto& config = reg.ctx().get<DifficultyConfig>();
    auto lp = reg.create();
    reg.emplace<ObstacleTag>(lp);
    reg.emplace<Position>(lp, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<Velocity>(lp, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(lp, ObstacleKind::LanePushLeft, int16_t{constants::PTS_LANE_PUSH});
    reg.emplace<DrawSize>(lp, float(constants::PLAYER_SIZE), 80.0f);
    reg.emplace<DrawLayer>(lp, Layer::Game);
    reg.emplace<Color>(lp, Color{100, 200, 100, 255});
    reg.emplace<ScoredTag>(lp);

    auto& score = reg.ctx().get<ScoreState>();
    int chain_before = score.chain_count;

    scoring_system(reg, 0.016f);

    CHECK(score.chain_count == chain_before);

    auto popup_view = reg.view<ScorePopup>();
    int popup_count = 0;
    for (auto e : popup_view) { ++popup_count; (void)e; }
    CHECK(popup_count == 0);

    CHECK_FALSE(reg.all_of<Obstacle>(lp));
    CHECK_FALSE(reg.all_of<ScoredTag>(lp));
}
