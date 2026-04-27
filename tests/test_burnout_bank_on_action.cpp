// Issue #239: burnout banking removed from player_input_system.
// Tests confirm: shape presses no longer attach BankedBurnout to obstacles,
// scoring always applies a flat 1.0× multiplier, and best_burnout stays 0.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

// ── AC#1: shape press no longer banks burnout on obstacle ────────────────────

TEST_CASE("no_burnout_bank: shape press does NOT attach BankedBurnout to obstacle",
          "[burnout_bank]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - 400.0f);

    auto btn = make_shape_button(reg, Shape::Circle);
    reg.ctx().get<entt::dispatcher>().enqueue<ButtonPressEvent>({btn});

    player_input_system(reg, 0.016f);

    CHECK_FALSE(reg.any_of<BankedBurnout>(obs));
}

TEST_CASE("no_burnout_bank: lane swipe does NOT attach BankedBurnout to obstacle",
          "[burnout_bank]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_lane_block(reg, 0b010, constants::PLAYER_Y - 300.0f);

    reg.ctx().get<entt::dispatcher>().enqueue<GoEvent>({Direction::Right});

    player_input_system(reg, 0.016f);

    CHECK_FALSE(reg.any_of<BankedBurnout>(obs));
}

// ── AC#2: scoring always uses flat 1.0× — BankedBurnout has no effect ────────

TEST_CASE("no_burnout_bank: scoring with manually-placed BankedBurnout still uses 1.0x",
          "[burnout_bank][ladder]") {
    // Even if something were to attach a BankedBurnout with MULT_CLUTCH (5×),
    // the scoring system no longer reads it — points equal base × timing_mult.
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<BankedBurnout>(obs, constants::MULT_CLUTCH, BurnoutZone::Dead);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // With burnout removed: points = base * 1.0 = 200 (plus tiny distance bonus)
    int pts = score.score - static_cast<int>(0.016f * constants::PTS_PER_SECOND);
    CHECK(pts == constants::PTS_SHAPE_GATE);
}

TEST_CASE("no_burnout_bank: scoring without BankedBurnout equals scoring with Danger bank",
          "[burnout_bank][ladder]") {
    auto reg1 = make_registry();
    auto obs1 = make_shape_gate(reg1, Shape::Circle, constants::PLAYER_Y);
    reg1.emplace<ScoredTag>(obs1);
    scoring_system(reg1, 0.016f);
    int no_bank_score = reg1.ctx().get<ScoreState>().score;

    auto reg2 = make_registry();
    auto obs2 = make_shape_gate(reg2, Shape::Circle, constants::PLAYER_Y);
    reg2.emplace<ScoredTag>(obs2);
    reg2.emplace<BankedBurnout>(obs2, constants::MULT_DANGER, BurnoutZone::Danger);
    scoring_system(reg2, 0.016f);
    int banked_score = reg2.ctx().get<ScoreState>().score;

    CHECK(no_bank_score == banked_score);
}

// ── AC#3: best_burnout stays 0 for the entire run ────────────────────────────

TEST_CASE("no_burnout_bank: best_burnout stays 0.0 after scoring multiple obstacles",
          "[burnout_bank]") {
    auto reg = make_registry();
    make_player(reg);

    for (int i = 0; i < 4; ++i) {
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y + float(i));
        reg.emplace<ScoredTag>(obs);
        scoring_system(reg, 0.016f);
    }

    auto& results = reg.ctx().get<SongResults>();
    CHECK_THAT(results.best_burnout, Catch::Matchers::WithinAbs(0.0f, 0.001f));
}

// ── AC#4: LanePush still excluded from chain/popup/best_burnout (unchanged) ──

TEST_CASE("no_burnout_bank: LanePush still excluded from chain and popup",
          "[burnout_bank][lane_push]") {
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
