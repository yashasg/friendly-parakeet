#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "systems/burnout_helpers.h"

// ── Helper: make a lane-push obstacle (LanePushLeft) ─────────────────────────

static entt::entity make_lane_push(entt::registry& reg, float y) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(obs, ObstacleKind::LanePushLeft, int16_t{constants::PTS_LANE_PUSH});
    reg.emplace<DrawSize>(obs, float(constants::PLAYER_SIZE), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{100, 200, 100, 255});
    return obs;
}

// ── Helper: simulate a shape-press that banks burnout at a given distance ─────

static entt::entity setup_gate_with_bank(entt::registry& reg, float dist) {
    make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - dist);
    float scale = reg.ctx().get<DifficultyConfig>().burnout_window_scale;
    auto sample = compute_burnout_for_distance(dist, scale);
    float mult  = multiplier_for_zone(sample.zone);
    reg.emplace<BankedBurnout>(obs, mult, sample.zone);
    return obs;
}

// ── AC#1: Four tiers reachable through banking ────────────────────────────────

TEST_CASE("bank_on_action: dist=600 yields Safe multiplier [1.0,1.5]",
          "[burnout_bank][ladder]") {
    auto reg = make_registry();
    auto obs = setup_gate_with_bank(reg, 600.0f);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // Base 200 * MULT_SAFE(1.0) = 200
    float mult_used = static_cast<float>(score.score - static_cast<int>(0.016f * constants::PTS_PER_SECOND))
                      / float(constants::PTS_SHAPE_GATE);
    CHECK(mult_used >= 1.0f);
    CHECK(mult_used <= 1.5f);
}

TEST_CASE("bank_on_action: dist=400 yields Risky multiplier [1.5,2.0]",
          "[burnout_bank][ladder]") {
    auto reg = make_registry();
    auto obs = setup_gate_with_bank(reg, 400.0f);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    float mult_used = static_cast<float>(score.score - static_cast<int>(0.016f * constants::PTS_PER_SECOND))
                      / float(constants::PTS_SHAPE_GATE);
    CHECK(mult_used >= 1.5f);
    CHECK(mult_used <= 2.0f);
}

TEST_CASE("bank_on_action: dist=200 yields Danger multiplier [2.0,5.0]",
          "[burnout_bank][ladder]") {
    auto reg = make_registry();
    auto obs = setup_gate_with_bank(reg, 200.0f);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    float mult_used = static_cast<float>(score.score - static_cast<int>(0.016f * constants::PTS_PER_SECOND))
                      / float(constants::PTS_SHAPE_GATE);
    CHECK(mult_used >= 2.0f);
    CHECK(mult_used <= 5.0f);
}

TEST_CASE("bank_on_action: dist=50 yields Dead multiplier == 5.0",
          "[burnout_bank][ladder]") {
    auto reg = make_registry();
    auto obs = setup_gate_with_bank(reg, 50.0f);
    reg.emplace<ScoredTag>(obs);

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    float mult_used = static_cast<float>(score.score - static_cast<int>(0.016f * constants::PTS_PER_SECOND))
                      / float(constants::PTS_SHAPE_GATE);
    CHECK_THAT(mult_used, Catch::Matchers::WithinAbs(5.0f, 0.01f));
}

// ── AC#2: best_burnout tracks highest bank seen ───────────────────────────────

TEST_CASE("bank_on_action: best_burnout is max of all banked multipliers",
          "[burnout_bank]") {
    auto reg = make_registry();
    make_player(reg);

    // Score four obstacles at increasing proximity
    float dists[4] = { 600.0f, 400.0f, 200.0f, 50.0f };
    for (float d : dists) {
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - d);
        float scale  = reg.ctx().get<DifficultyConfig>().burnout_window_scale;
        auto sample  = compute_burnout_for_distance(d, scale);
        float mult   = multiplier_for_zone(sample.zone);
        reg.emplace<BankedBurnout>(obs, mult, sample.zone);
        reg.emplace<ScoredTag>(obs);
        scoring_system(reg, 0.016f);
    }

    auto& results = reg.ctx().get<SongResults>();
    CHECK_THAT(results.best_burnout, Catch::Matchers::WithinAbs(5.0f, 0.01f));
}

TEST_CASE("bank_on_action: safe-only run best_burnout is in [1.0,1.5]",
          "[burnout_bank]") {
    auto reg = make_registry();
    make_player(reg);

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - 600.0f);
    float scale  = reg.ctx().get<DifficultyConfig>().burnout_window_scale;
    auto sample  = compute_burnout_for_distance(600.0f, scale);
    float mult   = multiplier_for_zone(sample.zone);
    reg.emplace<BankedBurnout>(obs, mult, sample.zone);
    reg.emplace<ScoredTag>(obs);
    scoring_system(reg, 0.016f);

    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.best_burnout >= 1.0f);
    CHECK(results.best_burnout <= 1.5f);
}

// ── AC#3: no-op clear defaults to MULT_SAFE, not MULT_CLUTCH ─────────────────

TEST_CASE("bank_on_action: no-op clear (no press) uses MULT_SAFE floor",
          "[burnout_bank]") {
    // Obstacle clears without any BankedBurnout — player was already in shape.
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    // No BankedBurnout emplace — no-op clear

    scoring_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    // Points = base * MULT_SAFE * timing_mult(1.0) = 200
    // Must NOT be 200*5 = 1000 (MULT_CLUTCH)
    float mult_used = static_cast<float>(score.score - static_cast<int>(0.016f * constants::PTS_PER_SECOND))
                      / float(constants::PTS_SHAPE_GATE);
    CHECK_THAT(mult_used, Catch::Matchers::WithinAbs(constants::MULT_SAFE, 0.01f));
}

// ── AC#4: LanePush is out of ladder ──────────────────────────────────────────

TEST_CASE("bank_on_action: LanePush does not contribute to chain, popup, or best_burnout",
          "[burnout_bank][lane_push]") {
    auto reg = make_registry();
    make_player(reg);

    auto lp = make_lane_push(reg, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(lp);

    auto& score = reg.ctx().get<ScoreState>();
    int chain_before = score.chain_count;

    scoring_system(reg, 0.016f);

    // Chain must not have incremented
    CHECK(score.chain_count == chain_before);
    // No ScorePopup spawned
    auto popup_view = reg.view<ScorePopup>();
    int popup_count = 0;
    for (auto e : popup_view) { ++popup_count; (void)e; }
    CHECK(popup_count == 0);
    // best_burnout must remain at its default (0.0)
    auto& results = reg.ctx().get<SongResults>();
    CHECK(results.best_burnout == 0.0f);
    // Entity cleaned up
    CHECK_FALSE(reg.all_of<Obstacle>(lp));
    CHECK_FALSE(reg.all_of<ScoredTag>(lp));
}

// ── AC#5: First-commit-locks for ComboGate ────────────────────────────────────
// ComboGate is ideal: player_input_system banks it on BOTH ButtonPressEvent
// (shape press) AND GoEvent (lane swipe), so both paths share the same lock.
// This test drives the system with real events — no manual BankedBurnout emplace.

TEST_CASE("bank_on_action: first-commit-locks — shape press at Safe preserved after lane swipe at Danger",
          "[burnout_bank][first_commit_locks]") {
    auto reg = make_registry();
    make_player(reg);

    // Gate at dist=600 → Safe zone (ZONE_SAFE_MIN=500 < 600 < ZONE_SAFE_MAX=700)
    auto gate = make_combo_gate(reg, Shape::Circle, /*blocked_mask=*/0b010,
                                constants::PLAYER_Y - 600.0f);

    auto& eq = reg.ctx().get<EventQueue>();

    // ── Pass 1: ButtonPressEvent at dist=600 (Safe) ───────────────────────────
    auto btn = make_shape_button(reg, Shape::Circle);
    eq.push_press(btn);
    player_input_system(reg, 0.016f);
    // player_input_system clears eq.press_count after processing

    REQUIRE(reg.any_of<BankedBurnout>(gate));
    {
        auto& b = reg.get<BankedBurnout>(gate);
        CHECK(b.zone == BurnoutZone::Safe);
        CHECK_THAT(b.multiplier, Catch::Matchers::WithinAbs(constants::MULT_SAFE, 0.01f));
    }

    // ── Advance gate to dist=200 (Danger zone: ZONE_DANGER_MIN=140 < 200 < ZONE_RISKY_MIN=300)
    reg.get<Position>(gate).y = constants::PLAYER_Y - 200.0f;

    // ── Pass 2: GoEvent (lane swipe) at dist=200 (Danger) ─────────────────────
    // GoEvent also triggers bank_burnout on ComboGate; lock must refuse overwrite.
    eq.push_go(Direction::Right);
    player_input_system(reg, 0.016f);

    // Safe bank must survive — this assertion fails if the first-commit-lock guard
    // (reg.any_of<BankedBurnout>(target)) is removed from player_input_system.
    REQUIRE(reg.any_of<BankedBurnout>(gate));
    auto& banked = reg.get<BankedBurnout>(gate);
    CHECK(banked.zone == BurnoutZone::Safe);
    CHECK_THAT(banked.multiplier, Catch::Matchers::WithinAbs(constants::MULT_SAFE, 0.01f));
}

// ── Additional: bank on shape-press via player_input_system ──────────────────

TEST_CASE("bank_on_action: shape press banks multiplier on nearest relevant obstacle",
          "[burnout_bank][integration]") {
    auto reg = make_registry();
    make_player(reg);
    // Place obstacle at dist=400 (Risky)
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - 400.0f);

    auto& eq = reg.ctx().get<EventQueue>();
    auto btn = make_shape_button(reg, Shape::Circle);
    eq.push_press(btn);

    player_input_system(reg, 0.016f);

    REQUIRE(reg.any_of<BankedBurnout>(obs));
    auto& banked = reg.get<BankedBurnout>(obs);
    CHECK(banked.zone == BurnoutZone::Risky);
    CHECK_THAT(banked.multiplier, Catch::Matchers::WithinAbs(constants::MULT_RISKY, 0.01f));
}

TEST_CASE("bank_on_action: BankedBurnout removed from entity after scoring",
          "[burnout_bank]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<BankedBurnout>(obs, constants::MULT_DANGER, BurnoutZone::Danger);

    scoring_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<BankedBurnout>(obs));
}
