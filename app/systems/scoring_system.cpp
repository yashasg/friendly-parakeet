#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/obstacle.h"
#include "gameplay_intents.h"
#include "../components/particle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/rhythm.h"
#include "../tags/tags.h"
#include "../util/rhythm_math.h"
#include "../constants.h"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>

namespace {

void enqueue_energy_effect(entt::registry& reg, float delta, bool flash = false) {
    // Per-frame row table (issue #1627): each enqueued effect is its own
    // entity. `energy_system` walks the row table in insertion order and
    // destroys the rows. Replaces the former `PendingEnergyEffects::events`
    // `std::vector<Event>` array column (Fabian Principle 3).
    auto e = reg.create();
    reg.emplace<PendingEnergyEffectTag>(e);
    reg.emplace<EnergyDelta>(e, EnergyDelta{delta});
    if (flash) reg.emplace<EnergyFlashTag>(e);
}

// Per-tier traits — Fabian per-value table: each specialization carries the
// row of constants the scoring transform reads for that tier (energy delta,
// results counter member-pointer, particle color, popup-request tier tag).
// One specialization per former TimingTier value; the prior 4-way switches
// are replaced by template selection over the tier tag.
template <typename TierTag>
struct tier_score_traits;

template <>
struct tier_score_traits<TimingPerfectTag> {
    static constexpr float multiplier          = 1.50f;
    static constexpr float energy_delta_signed = constants::ENERGY_RECOVER_PERFECT;
    static constexpr bool  energy_flash        = false;
    static constexpr Color particle_color{255, 230, 90, 240};
    static constexpr int   SongResults::* results_counter = &SongResults::perfect_count;
    using PopupRequestTag = PopupRequestTierPerfectTag;
};

template <>
struct tier_score_traits<TimingGoodTag> {
    static constexpr float multiplier          = 1.00f;
    static constexpr float energy_delta_signed = constants::ENERGY_RECOVER_GOOD;
    static constexpr bool  energy_flash        = false;
    static constexpr Color particle_color{120, 255, 160, 230};
    static constexpr int   SongResults::* results_counter = &SongResults::good_count;
    using PopupRequestTag = PopupRequestTierGoodTag;
};

template <>
struct tier_score_traits<TimingOkTag> {
    static constexpr float multiplier          = 0.50f;
    static constexpr float energy_delta_signed = constants::ENERGY_RECOVER_OK;
    static constexpr bool  energy_flash        = false;
    static constexpr Color particle_color{100, 180, 255, 220};
    static constexpr int   SongResults::* results_counter = &SongResults::ok_count;
    using PopupRequestTag = PopupRequestTierOkTag;
};

template <>
struct tier_score_traits<TimingBadTag> {
    static constexpr float multiplier          = 0.25f;
    static constexpr float energy_delta_signed = -constants::ENERGY_DRAIN_BAD;
    static constexpr bool  energy_flash        = true;
    static constexpr Color particle_color{255, 100, 100, 220};
    static constexpr int   SongResults::* results_counter = &SongResults::bad_count;
    using PopupRequestTag = PopupRequestTierBadTag;
};

constexpr Color kUntimedParticleColor{220, 220, 255, 220};

void spawn_score_particles(entt::registry& reg, const Vector2& position, Color color) {
    constexpr float kLifetime = 0.35f;
    constexpr float kSize = 10.0f;
    constexpr float kSpeed = 90.0f;
    constexpr Vector2 kDirections[] = {
        {1.0f, 0.0f},
        {-1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, -1.0f},
        {0.707f, 0.707f},
        {-0.707f, 0.707f},
    };

    for (const auto& dir : kDirections) {
        auto particle = reg.create();
        reg.emplace<ParticleTag>(particle);
        reg.emplace<ParticleData>(particle, kSize, kLifetime, kLifetime);
        reg.emplace<WorldPosition>(particle, WorldPosition{position});
        reg.emplace<Vector2>(particle, Vector2Scale(dir, kSpeed));
        reg.emplace<Color>(particle, color);
        reg.emplace<TagEffectsPass>(particle);
    }
}

float chain_multiplier_for_count(int32_t chain_count) {
    if (chain_count <= 1) {
        return 1.0f;
    }
    const int32_t bonus_steps = std::min(chain_count - 1, constants::CHAIN_MULT_BONUS_STEPS_CAP);
    return 1.0f + constants::CHAIN_MULT_STEP * static_cast<float>(bonus_steps);
}

// Process one tier of graded hits. Per-tier constants come from
// tier_score_traits<TierTag>; the chain/score/popup-queue logic is shared
// across tiers because that data is per-row (not per-tier).
//
// Two-phase pattern (Fabian Principle 3 / issue #1629): gather emplaces
// `PendingHitResolveTag` on each matching obstacle while iterating the
// gather view read-only (EnTT-iteration-safe). Drain iterates the disjoint
// `PendingHitResolveTag` storage, reading the per-row data via
// `reg.get<Obstacle>(e)` / `reg.get<WorldPosition>(e)` (still live on the
// entity until the strip step at the bottom of the loop body). The pending
// tag is batch-removed after the drain so the same tag can be reused by
// the next tier pass.
template <typename TierTag>
void process_tier_hit_pass(entt::registry& reg,
                           ScoreState& score,
                           SongResults* results) {
    {
        auto gather = reg.view<ObstacleTag, ScoredTag, Obstacle, WorldPosition, TimingGrade, TierTag>(
            entt::exclude<MissTag, NonScorableTag>);
        for (auto e : gather) {
            reg.emplace<PendingHitResolveTag>(e);
        }
    }

    auto drain = reg.view<PendingHitResolveTag>();
    if (drain.size() == 0u) return;

    using Traits = tier_score_traits<TierTag>;

    for (auto e : drain) {
        const auto& obs = reg.get<Obstacle>(e);
        const auto& wp  = reg.get<WorldPosition>(e);

        enqueue_energy_effect(reg, Traits::energy_delta_signed, Traits::energy_flash);
        if (results) {
            ((*results).*Traits::results_counter) += 1;
        }

        const bool contributes_to_chain = obs.base_points > 0;
        if (contributes_to_chain) {
            score.chain_count++;
        }
        const float chain_mult  = chain_multiplier_for_count(score.chain_count);
        const int points = static_cast<int>(
            std::floor(static_cast<float>(obs.base_points) * Traits::multiplier * chain_mult));

        if (contributes_to_chain && results && score.chain_count > results->max_chain) {
            results->max_chain = score.chain_count;
        }

        score.score += points;

        enqueue_popup_request<typename Traits::PopupRequestTag>(reg, wp.position.x, wp.position.y, points);
        spawn_score_particles(reg, wp.position, Traits::particle_color);

        reg.get_or_emplace<ResolvedObstacleTag>(e);
        reg.remove<Obstacle>(e);
        reg.remove<ScoredTag>(e);
        remove_timing_grade_and_tags(reg, e);
    }

    reg.remove<PendingHitResolveTag>(drain.begin(), drain.end());
}

void process_ungraded_hit_pass(entt::registry& reg,
                               ScoreState& score,
                               SongResults* results) {
    {
        auto gather = reg.view<ObstacleTag, ScoredTag, Obstacle, WorldPosition>(
            entt::exclude<MissTag, NonScorableTag, TimingGrade>);
        for (auto e : gather) {
            reg.emplace<PendingHitResolveTag>(e);
        }
    }

    auto drain = reg.view<PendingHitResolveTag>();
    if (drain.size() == 0u) return;

    for (auto e : drain) {
        const auto& obs = reg.get<Obstacle>(e);
        const auto& wp  = reg.get<WorldPosition>(e);

        const bool contributes_to_chain = obs.base_points > 0;
        if (contributes_to_chain) {
            score.chain_count++;
        }
        const float chain_mult = chain_multiplier_for_count(score.chain_count);
        // Ungraded hits never had a tier — pre-migration the switch fell
        // through with timing_mult = 1.0f. Preserve that exactly.
        const int points = static_cast<int>(
            std::floor(static_cast<float>(obs.base_points) * chain_mult));

        if (contributes_to_chain && results && score.chain_count > results->max_chain) {
            results->max_chain = score.chain_count;
        }

        score.score += points;

        enqueue_popup_request<PopupRequestTierUntimedTag>(reg, wp.position.x, wp.position.y, points);
        spawn_score_particles(reg, wp.position, kUntimedParticleColor);

        reg.get_or_emplace<ResolvedObstacleTag>(e);
        reg.remove<Obstacle>(e);
        reg.remove<ScoredTag>(e);
    }

    reg.remove<PendingHitResolveTag>(drain.begin(), drain.end());
}

}  // namespace

void scoring_system(entt::registry& reg, float dt) {
    auto& score   = reg.ctx().get<ScoreState>();

    // Passive score accrual (PTS_PER_SECOND) only while song playback is active.
    // Per Fabian Principle 3 / issue #1624: absence of `SongFinishedTag`
    // IS "song has not finished yet" — covers both the "no song / pre-play"
    // case (which used the former `song == nullptr` proxy) and the
    // "song actively playing" case in a single tag check.
    const bool allow_passive_score = !reg.ctx().contains<SongFinishedTag>();
    if (allow_passive_score) {
        const float passive_score = score.passive_score_remainder +
            dt * static_cast<float>(constants::PTS_PER_SECOND);
        const int whole_points = static_cast<int>(std::floor(passive_score));
        score.score += whole_points;
        score.passive_score_remainder = passive_score - static_cast<float>(whole_points);
    }

    auto* results = reg.ctx().find<SongResults>();   // #309: hoisted above loop

    // Miss pass: single owner of ENERGY_DRAIN_MISS and miss_count.
    // Gather → drain via `PendingMissResolveTag` row table (issue #1629):
    // gather is read-only over the obstacle view (EnTT-iteration-safe);
    // drain iterates the disjoint pending storage and strips Obstacle/
    // ScoredTag/MissTag/TimingGrade safely. `remove_timing_grade_and_tags`
    // is idempotent (a `remove<T>` on an absent component is a no-op), so
    // both graded and ungraded miss rows can call it unconditionally —
    // dropping the former `MissRecord::has_timing` bool entirely.
    {
        auto miss_graded = reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle, TimingGrade>(
            entt::exclude<NonScorableTag>);
        for (auto e : miss_graded) {
            enqueue_energy_effect(reg, -constants::ENERGY_DRAIN_MISS, true);
            if (results) results->miss_count++;
            score.chain_count = 0;
            reg.emplace<PendingMissResolveTag>(e);
        }

        auto miss_ungraded = reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle>(
            entt::exclude<TimingGrade, NonScorableTag>);
        for (auto e : miss_ungraded) {
            enqueue_energy_effect(reg, -constants::ENERGY_DRAIN_MISS, true);
            if (results) results->miss_count++;
            score.chain_count = 0;
            reg.emplace<PendingMissResolveTag>(e);
        }

        auto miss_drain = reg.view<PendingMissResolveTag>();
        for (auto e : miss_drain) {
            reg.get_or_emplace<ResolvedObstacleTag>(e);
            reg.remove<Obstacle>(e);
            reg.remove<ScoredTag>(e);
            reg.remove<MissTag>(e);
            remove_timing_grade_and_tags(reg, e);
        }
        reg.remove<PendingMissResolveTag>(miss_drain.begin(), miss_drain.end());
    }

    // Hit pass: one transform per former TimingTier value plus one ungraded
    // transform (#1202/#1204). Per-tier constants live in tier_score_traits;
    // no `switch` on a discriminator anywhere in the hit pipeline.
    process_tier_hit_pass<TimingPerfectTag>(reg, score, results);
    process_tier_hit_pass<TimingGoodTag>   (reg, score, results);
    process_tier_hit_pass<TimingOkTag>     (reg, score, results);
    process_tier_hit_pass<TimingBadTag>    (reg, score, results);
    process_ungraded_hit_pass              (reg, score, results);

    // ── NonScorable cleanup ───────────────────────────────────────────────────
    // Entities excluded from the hit pass via NonScorableTag still need their
    // ScoredTag and Obstacle stripped after collision resolution so they are not
    // re-processed on the next frame. Gather → drain via
    // `PendingNonScorableCleanupTag` row table (issue #1629).
    {
        auto gather = reg.view<ObstacleTag, ScoredTag, NonScorableTag>();
        for (auto e : gather) {
            reg.emplace<PendingNonScorableCleanupTag>(e);
        }
        auto drain = reg.view<PendingNonScorableCleanupTag>();
        for (auto e : drain) {
            reg.get_or_emplace<ResolvedObstacleTag>(e);
            reg.remove<Obstacle>(e);
            reg.remove<ScoredTag>(e);
            reg.remove<MissTag>(e);
            remove_timing_grade_and_tags(reg, e);
        }
        reg.remove<PendingNonScorableCleanupTag>(drain.begin(), drain.end());
    }

    // Smooth score display
    auto& display = reg.ctx().get<ScoreDisplay>();
    float display_speed = 5000.0f * dt;
    if (display.displayed < score.score) {
        display.displayed += static_cast<int>(display_speed);
        if (display.displayed > score.score)
            display.displayed = score.score;
    }
}
