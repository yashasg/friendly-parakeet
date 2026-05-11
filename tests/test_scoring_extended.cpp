#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── scoring_system: timing multiplier integration ────────────

namespace {

int score_shape_gate_with_tier(entt::registry& reg, TimingTier tier) {
    auto& score = reg.ctx().get<ScoreState>();
    const int before = score.score;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, tier, 0.0f);

    scoring_system(reg, 0.0f);
    return score.score - before;
}

}  // namespace

TEST_CASE("scoring: perfect timing multiplier gives 1.5x base", "[scoring][rhythm]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Perfect, 1.0f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // 200 * 0.25 * 1.0 = 50, plus distance bonus
    // Should be less than a normal hit
    CHECK(score.score < constants::PTS_SHAPE_GATE + 50);
}

TEST_CASE("scoring: Good timing multiplier applies through scoring_system", "[scoring][rhythm][issue221]") {
    auto reg = make_registry();

    CHECK(score_shape_gate_with_tier(reg, TimingTier::Good) == constants::PTS_SHAPE_GATE);
}

TEST_CASE("scoring: Ok timing multiplier applies through scoring_system", "[scoring][rhythm][issue221]") {
    auto reg = make_registry();

    CHECK(score_shape_gate_with_tier(reg, TimingTier::Ok) == constants::PTS_SHAPE_GATE / 2);
}

TEST_CASE("scoring: Bad timing multiplier applies through scoring_system and drains energy",
          "[scoring][rhythm][energy][issue221]") {
    auto reg = make_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = constants::ENERGY_MAX;

    CHECK(score_shape_gate_with_tier(reg, TimingTier::Bad) == constants::PTS_SHAPE_GATE / 4);

    energy_system(reg, 0.0f);

    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(constants::ENERGY_MAX - constants::ENERGY_DRAIN_BAD,
                                          0.0001f));
}

TEST_CASE("scoring: perfect timing and base points combine correctly", "[scoring][rhythm]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Perfect, 1.0f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);
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
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<TimingGrade>(obs));
}

TEST_CASE("scoring: popup timing_tier set for graded obstacles", "[scoring][rhythm]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    for (auto [e, popup] : popup_view.each()) {
        CHECK(popup.has_timing_tier);
        CHECK(popup.timing_tier == TimingTier::Good);
    }
}

TEST_CASE("scoring: popup timing_tier is absent for ungraded obstacles", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    // No TimingGrade

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    for (auto [e, popup] : popup_view.each()) {
        CHECK(!popup.has_timing_tier);
    }
}

TEST_CASE("scoring: multiple obstacles scored in single frame", "[scoring]") {
    auto reg = make_registry();

    auto obs1 = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs1);
    auto obs2 = make_lane_block(reg, 0b001, constants::PLAYER_Y + 1.0f);
    reg.emplace<ScoredTag>(obs2);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(score.chain_count == 0);
    CHECK(score.chain_timer == 0.0f);
    CHECK(score.score == score_before + static_cast<int>(0.016f * constants::PTS_PER_SECOND));
    CHECK_FALSE(reg.all_of<Obstacle>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
    CHECK_FALSE(reg.all_of<MissTag>(obs));
}

TEST_CASE("scoring: mixed miss and perfect at zero preserves per-event clamp ordering", "[scoring][energy]") {
    auto reg = make_registry();
    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.0f;

    auto miss = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(miss);
    reg.emplace<MissTag>(miss);

    auto hit = make_shape_gate(reg, Shape::Square, constants::PLAYER_Y + 1.0f);
    reg.emplace<ScoredTag>(hit);
    reg.emplace<TimingGrade>(hit, TimingTier::Perfect, 1.0f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_THAT(energy.energy, Catch::Matchers::WithinAbs(constants::ENERGY_RECOVER_PERFECT, 0.0001f));
}
