#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "components/system_scratch.h"

namespace {

int score_good_shape_gate(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    const int before = score.score;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.0f);
    scoring_system(reg, 0.0f);

    return score.score - before;
}

}  // namespace

namespace {

struct ScoredTierResult {
    int points = 0;
    float energy_delta = 0.0f;
};

ScoredTierResult score_single_shape_gate_with_tier(TimingTier tier) {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& energy = reg.ctx().get<EnergyState>();
    const int score_before = score.score;
    const float energy_before = energy.energy;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, tier, 0.0f);

    scoring_system(reg, 0.0f);
    energy_system(reg, 0.0f);

    return ScoredTierResult{score.score - score_before, energy.energy - energy_before};
}

}  // namespace

TEST_CASE("scoring: distance bonus accumulates", "[scoring]") {
    auto reg = make_registry();

    scoring_system(reg, 1.0f);
    popup_feedback_system(reg, 1.0f);
    energy_system(reg, 1.0f);

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.score >= constants::PTS_PER_SECOND);
    CHECK(score.distance_traveled > 0.0f);
}

TEST_CASE("scoring: distance bonus accumulates across fixed ticks", "[scoring][issue764]") {
    auto reg = make_registry();

    constexpr float fixed_dt = 1.0f / 60.0f;
    for (int tick = 0; tick < 60; ++tick) {
        scoring_system(reg, fixed_dt);
    }

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.score == constants::PTS_PER_SECOND);
    CHECK(score.passive_score_remainder >= 0.0f);
    CHECK(score.passive_score_remainder < 1.0f);
}

TEST_CASE("scoring: scored obstacle awards points", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.score >= constants::PTS_SHAPE_GATE);
}

TEST_CASE("scoring: chain multiplier increases points", "[scoring]") {
    auto reg = make_registry();

    // Score 3 obstacles in a row
    for (int i = 0; i < 3; ++i) {
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y + float(i));
        reg.emplace<ScoredTag>(obs);
        reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);
        scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);
    }

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.chain_count == 3);
    // Total should be more than 3x base due to the chain multiplier.
    int base_only = 3 * constants::PTS_SHAPE_GATE;
    CHECK(score.score > base_only);
}

TEST_CASE("scoring: chain persists across authored rests until miss (#100)", "[scoring][issue100]") {
    auto reg = make_registry();

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);
    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);
    CHECK(reg.ctx().get<ScoreState>().chain_count == 1);

    // Musical rests should not silently break a clean chain.
    scoring_system(reg, 2.5f);
    popup_feedback_system(reg, 2.5f);
    energy_system(reg, 2.5f);

    CHECK(reg.ctx().get<ScoreState>().chain_count == 1);

    auto miss = make_shape_gate(reg, Shape::Square, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(miss);
    reg.emplace<MissTag>(miss);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(reg.ctx().get<ScoreState>().chain_count == 0);
}

TEST_CASE("scoring: popup entity spawned on score", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    int popup_count = 0;
    for (auto e : popup_view) {
        CHECK(reg.all_of<WorldTransform>(e));
        CHECK(reg.all_of<MotionVelocity>(e));
        CHECK(reg.all_of<TagHUDPass>(e));
        ++popup_count;
    }
    CHECK(popup_count == 1);
}

TEST_CASE("scoring: score feedback spawns effect particles", "[scoring][particle][issue782]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Perfect, 1.0f);

    scoring_system(reg, 0.016f);

    int particle_count = 0;
    auto particle_view =
        reg.view<ParticleTag, ParticleData, WorldTransform, MotionVelocity, Color, TagEffectsPass>();
    for (auto [entity, particle, transform, velocity, color] : particle_view.each()) {
        CHECK(reg.valid(entity));
        CHECK(particle.remaining > 0.0f);
        CHECK(particle.max_time > 0.0f);
        CHECK(transform.position.y == constants::PLAYER_Y);
        CHECK((velocity.value.x != 0.0f || velocity.value.y != 0.0f));
        CHECK(color.a > 0);
        ++particle_count;
    }

    CHECK(particle_count > 0);
}

TEST_CASE("scoring: SFX pushed on score", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(drain_sfx_events(reg).count > 0);
}

TEST_CASE("scoring: displayed_score rolls up toward score", "[scoring]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 1000;
    score.displayed_score = 0;

    scoring_system(reg, 0.1f);
    popup_feedback_system(reg, 0.1f);
    energy_system(reg, 0.1f);

    CHECK(score.displayed_score > 0);
    CHECK(score.displayed_score <= score.score);
}

TEST_CASE("scoring: not in Playing phase skips processing", "[scoring]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;

    tick_playing_systems(reg, 1.0f);

    CHECK(reg.ctx().get<ScoreState>().score == 0);
}

TEST_CASE("scoring: chain multiplier 5+ gives extended value", "[scoring]") {
    auto reg = make_registry();

    // Score 5 obstacles in a row (chain_count 1..5)
    for (int i = 0; i < 5; ++i) {
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y + float(i));
        reg.emplace<ScoredTag>(obs);
        reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);
        scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);
    }

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.chain_count == 5);
    // Total for 5 obstacles should exceed base-only scoring due to the chain multiplier.
    CHECK(score.score > 5 * constants::PTS_SHAPE_GATE);
}

TEST_CASE("scoring: chain multiplier economy scales at design checkpoints (#206)", "[scoring]") {
    auto reg = make_registry();

    int chain_1_points = 0;
    int chain_5_points = 0;
    int chain_10_points = 0;
    int chain_20_points = 0;

    for (int chain = 1; chain <= 20; ++chain) {
        const int points = score_good_shape_gate(reg);
        if (chain == 1) chain_1_points = points;
        if (chain == 5) chain_5_points = points;
        if (chain == 10) chain_10_points = points;
        if (chain == 20) chain_20_points = points;
    }

    CHECK(chain_1_points == 200);
    CHECK(chain_5_points == 240);
    CHECK(chain_10_points == 290);
    CHECK(chain_20_points == 390);
    CHECK(chain_1_points < chain_5_points);
    CHECK(chain_5_points < chain_10_points);
    CHECK(chain_10_points < chain_20_points);
}

TEST_CASE("scoring: breaking a 10-chain loses meaningful next-hit value (#206)", "[scoring]") {
    auto reg = make_registry();

    for (int i = 0; i < 9; ++i) {
        (void)score_good_shape_gate(reg);
    }
    const int chained_tenth_hit = score_good_shape_gate(reg);

    auto miss = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(miss);
    reg.emplace<MissTag>(miss);
    scoring_system(reg, 0.0f);
    REQUIRE(reg.ctx().get<ScoreState>().chain_count == 0);

    const int isolated_after_break = score_good_shape_gate(reg);

    CHECK(chained_tenth_hit == 290);
    CHECK(isolated_after_break == 200);
    CHECK(chained_tenth_hit >= isolated_after_break + 90);
}

TEST_CASE("scoring: obstacle entity cleaned up after scoring", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    popup_feedback_system(reg, 10.0f);
    energy_system(reg, 10.0f);

    // displayed_score should not exceed score (capped)
    // Note: score increases by dt * PTS_PER_SECOND too
    CHECK(score.displayed_score <= score.score);
}

TEST_CASE("scoring: distance_traveled accumulates from scroll speed", "[scoring]") {
    auto reg = make_registry();
    auto& song = reg.ctx().get<SongState>();
    song.scroll_speed = 400.0f;

    scoring_system(reg, 1.0f);
    popup_feedback_system(reg, 1.0f);
    energy_system(reg, 1.0f);

    CHECK(reg.ctx().get<ScoreState>().distance_traveled == 400.0f);
}

// On-beat shape gate scores at base points (timing only, no burnout multiplier).
TEST_CASE("scoring: no-penalty — on-beat gate scores at base points", "[scoring]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.0f);
    popup_feedback_system(reg, 0.0f);
    energy_system(reg, 0.0f);  // dt=0 excludes distance bonus for exact assertion

    auto& score = reg.ctx().get<ScoreState>();
    CHECK(score.score == constants::PTS_SHAPE_GATE);
}

TEST_CASE("scoring: timing multiplier applies end-to-end for non-perfect tiers (#221)", "[scoring][issue221]") {
    const auto good = score_single_shape_gate_with_tier(TimingTier::Good);
    CHECK(good.points == 200);

    const auto ok = score_single_shape_gate_with_tier(TimingTier::Ok);
    CHECK(ok.points == 100);

    const auto bad = score_single_shape_gate_with_tier(TimingTier::Bad);
    CHECK(bad.points == 50);
    CHECK_THAT(bad.energy_delta, Catch::Matchers::WithinAbs(-constants::ENERGY_DRAIN_BAD, 0.0001f));
}

TEST_CASE("scoring: popup entity has full factory contract", "[scoring][popup_entity]") {
    auto reg = make_registry();
    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 0.016f);
    popup_feedback_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    auto popup_view = reg.view<ScorePopup>();
    int count = 0;
    for (auto e : popup_view) {
        ++count;
        CHECK(reg.all_of<WorldTransform>(e));
        CHECK(reg.all_of<MotionVelocity>(e));
        CHECK(reg.all_of<Color>(e));
        CHECK(reg.all_of<DrawLayer>(e));
        CHECK(reg.all_of<TagHUDPass>(e));
        CHECK(reg.all_of<PopupDisplay>(e));

        const auto& mv = reg.get<MotionVelocity>(e);
        CHECK(mv.value.x == 0.0f);
        CHECK(mv.value.y == -80.0f);

        const auto& dl = reg.get<DrawLayer>(e);
        CHECK(dl.layer == Layer::Effects);
    }
    CHECK(count == 1);
}

TEST_CASE("scoring: NonScorableTag entity cleared without scoring", "[scoring][nonscorable]") {
    // Verifies OCP: any entity with NonScorableTag is excluded from the scoring
    // ladder regardless of its structural obstacle archetype.
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    const int score_before = score.score;
    const int chain_before = score.chain_count;

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<WorldTransform>(e, WorldTransform{{300.0f, constants::PLAYER_Y}});
    reg.emplace<Obstacle>(e, int16_t{999});
    reg.emplace<NonScorableTag>(e);
    reg.emplace<ScoredTag>(e);

    scoring_system(reg, 0.0f);

    // No score delta from obstacle points.
    CHECK(score.score == score_before);
    // Chain not incremented.
    CHECK(score.chain_count == chain_before);
    // No popup spawned.
    CHECK(reg.view<ScorePopup>().size() == 0);
    // ScoredTag and Obstacle cleaned up — entity won't be re-processed.
    CHECK_FALSE(reg.all_of<ScoredTag>(e));
    CHECK_FALSE(reg.all_of<Obstacle>(e));
}

TEST_CASE("scoring: missed NonScorableTag entity is resolved without effects",
          "[scoring][nonscorable][issue965]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    auto& energy = reg.ctx().get<EnergyState>();
    auto& results = reg.ctx().get<SongResults>();
    const int score_before = score.score;
    const int chain_before = score.chain_count;
    const float energy_before = energy.energy;

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<WorldTransform>(e, WorldTransform{{300.0f, constants::PLAYER_Y}});
    reg.emplace<Obstacle>(e, int16_t{0});
    reg.emplace<NonScorableTag>(e);
    reg.emplace<ScoredTag>(e);
    reg.emplace<MissTag>(e);
    reg.emplace<TimingGrade>(e, TimingTier::Bad, 0.0f);

    scoring_system(reg, 0.0f);
    energy_system(reg, 0.0f);

    CHECK(score.score == score_before);
    CHECK(score.chain_count == chain_before);
    CHECK(energy.energy == energy_before);
    CHECK(results.miss_count == 0);
    CHECK(reg.view<ScorePopup>().size() == 0);
    CHECK(reg.all_of<ResolvedObstacleTag>(e));
    CHECK_FALSE(reg.all_of<Obstacle>(e));
    CHECK_FALSE(reg.all_of<ScoredTag>(e));
    CHECK_FALSE(reg.all_of<MissTag>(e));
    CHECK_FALSE(reg.all_of<TimingGrade>(e));
}

TEST_CASE("scoring: missed obstacle drains energy without setting terminal death cause", "[scoring]") {
    auto reg = make_registry();
    auto& gos = reg.ctx().get<GameOverState>();
    REQUIRE(gos.cause == DeathCause::None);

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<WorldTransform>(e, WorldTransform{{300.0f, constants::PLAYER_Y}});
    reg.emplace<Obstacle>(e, int16_t{200});
    reg.emplace<ScoredTag>(e);
    reg.emplace<MissTag>(e);

    scoring_system(reg, 0.0f);

    CHECK(gos.cause == DeathCause::None);
}

TEST_CASE("scoring: passive accrual stops once song playback has finished", "[scoring][issue445]") {
    auto reg = make_registry();
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.playing = false;

    auto& score = reg.ctx().get<ScoreState>();
    score.score = 500;
    score.distance_traveled = 123.0f;

    scoring_system(reg, 1.0f);

    CHECK(score.score == 500);
    CHECK(score.distance_traveled == 123.0f);
}

TEST_CASE("scoring: obstacle/timing points still apply after playback has finished", "[scoring][issue445]") {
    auto reg = make_registry();
    auto& song = reg.ctx().get<SongState>();
    song.finished = true;
    song.playing = false;

    auto& score = reg.ctx().get<ScoreState>();
    score.score = 0;

    auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(obs);
    reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);

    scoring_system(reg, 1.0f);

    CHECK(score.score >= constants::PTS_SHAPE_GATE);
}

TEST_CASE("runtime scratch: dense scoring burst stays within reserved capacity", "[scoring][issue557]") {
    auto reg = make_registry();
    constexpr int dense_count = 6;
    runtime_system_scratch_reserve(reg, dense_count);

    auto& scratch = reg.ctx().get<ScoringSystemScratch>();
    auto& energy = reg.ctx().get<PendingEnergyEffects>();
    auto& popup_queue = reg.ctx().get<ScorePopupRequestQueue>();
    const auto hit_capacity = scratch.hit_buf.capacity();
    const auto energy_capacity = energy.events.capacity();
    const auto popup_capacity = popup_queue.requests.capacity();

    for (int i = 0; i < dense_count; ++i) {
        auto obs = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y + static_cast<float>(i));
        reg.emplace<ScoredTag>(obs);
        reg.emplace<TimingGrade>(obs, TimingTier::Good, 0.5f);
    }

    scoring_system(reg, 0.0f);

    CHECK(scratch.hit_buf.capacity() == hit_capacity);
    CHECK(energy.events.capacity() == energy_capacity);
    CHECK(popup_queue.requests.capacity() == popup_capacity);
    CHECK(scratch.hit_capacity_exceeded_count == 0);
    CHECK(energy.capacity_exceeded_count == 0);
    CHECK(popup_queue.capacity_exceeded_count == 0);
    CHECK(energy.events.size() == static_cast<std::size_t>(dense_count));
    CHECK(popup_queue.requests.size() == static_cast<std::size_t>(dense_count));
}
