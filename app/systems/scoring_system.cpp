#include "all_systems.h"
#include "scoring_system.h"
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
    auto& pending = reg.ctx().get<PendingEnergyEffects>();
    if (pending.events.size() >= pending.events.capacity()) {
        ++pending.capacity_exceeded_count;
    }
    pending.events.push_back(PendingEnergyEffects::Event{delta, flash});
}

// Per-tier traits — Fabian per-value table: each specialization carries the
// row of constants the scoring transform reads for that tier (energy delta,
// results counter member-pointer, particle color, popup-queue member-pointer).
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
    static constexpr std::vector<TimedPopupRequest> ScorePopupRequestQueue::* queue =
        &ScorePopupRequestQueue::perfect;
};

template <>
struct tier_score_traits<TimingGoodTag> {
    static constexpr float multiplier          = 1.00f;
    static constexpr float energy_delta_signed = constants::ENERGY_RECOVER_GOOD;
    static constexpr bool  energy_flash        = false;
    static constexpr Color particle_color{120, 255, 160, 230};
    static constexpr int   SongResults::* results_counter = &SongResults::good_count;
    static constexpr std::vector<TimedPopupRequest> ScorePopupRequestQueue::* queue =
        &ScorePopupRequestQueue::good;
};

template <>
struct tier_score_traits<TimingOkTag> {
    static constexpr float multiplier          = 0.50f;
    static constexpr float energy_delta_signed = constants::ENERGY_RECOVER_OK;
    static constexpr bool  energy_flash        = false;
    static constexpr Color particle_color{100, 180, 255, 220};
    static constexpr int   SongResults::* results_counter = &SongResults::ok_count;
    static constexpr std::vector<TimedPopupRequest> ScorePopupRequestQueue::* queue =
        &ScorePopupRequestQueue::ok;
};

template <>
struct tier_score_traits<TimingBadTag> {
    static constexpr float multiplier          = 0.25f;
    static constexpr float energy_delta_signed = -constants::ENERGY_DRAIN_BAD;
    static constexpr bool  energy_flash        = true;
    static constexpr Color particle_color{255, 100, 100, 220};
    static constexpr int   SongResults::* results_counter = &SongResults::bad_count;
    static constexpr std::vector<TimedPopupRequest> ScorePopupRequestQueue::* queue =
        &ScorePopupRequestQueue::bad;
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
template <typename TierTag>
void process_tier_hit_pass(entt::registry& reg,
                           ScoreState& score,
                           SongResults* results,
                           ScoringSystemScratch& scratch,
                           ScorePopupRequestQueue& popup_queue) {
    auto& hit_buf = scratch.hit_buf;
    hit_buf.clear();

    auto view = reg.view<ObstacleTag, ScoredTag, Obstacle, WorldPosition, TimingGrade, TierTag>(
        entt::exclude<MissTag, NonScorableTag>);
    for (auto [e, obs, wt, tg] : view.each()) {
        HitRecord r;
        r.e          = e;
        r.popup_xy   = wt.position;
        r.obs        = obs;
        r.has_timing = true;
        r.precision  = tg.precision;
        if (hit_buf.size() >= hit_buf.capacity()) {
            ++scratch.hit_capacity_exceeded_count;
        }
        hit_buf.push_back(r);
    }

    if (hit_buf.empty()) return;

    using Traits = tier_score_traits<TierTag>;

    for (auto& r : hit_buf) {
        enqueue_energy_effect(reg, Traits::energy_delta_signed, Traits::energy_flash);
        if (results) {
            ((*results).*Traits::results_counter) += 1;
        }

        const bool contributes_to_chain = r.obs.base_points > 0;
        if (contributes_to_chain) {
            score.chain_count++;
        }
        const float chain_mult  = chain_multiplier_for_count(score.chain_count);
        int points = static_cast<int>(
            std::floor(static_cast<float>(r.obs.base_points) * Traits::multiplier * chain_mult));

        if (contributes_to_chain && results && score.chain_count > results->max_chain) {
            results->max_chain = score.chain_count;
        }

        score.score += points;

        auto& queue = popup_queue.*Traits::queue;
        if (queue.size() >= queue.capacity()) {
            ++popup_queue.capacity_exceeded_count;
        }
        queue.push_back({r.popup_xy.x, r.popup_xy.y, points});
        spawn_score_particles(reg, r.popup_xy, Traits::particle_color);

        reg.get_or_emplace<ResolvedObstacleTag>(r.e);
        reg.remove<Obstacle>(r.e);
        reg.remove<ScoredTag>(r.e);
        remove_timing_grade_and_tags(reg, r.e);
    }
}

void process_ungraded_hit_pass(entt::registry& reg,
                               ScoreState& score,
                               SongResults* results,
                               ScoringSystemScratch& scratch,
                               ScorePopupRequestQueue& popup_queue) {
    auto& hit_buf = scratch.hit_buf;
    hit_buf.clear();

    auto view = reg.view<ObstacleTag, ScoredTag, Obstacle, WorldPosition>(
        entt::exclude<MissTag, NonScorableTag, TimingGrade>);
    for (auto [e, obs, wt] : view.each()) {
        HitRecord r;
        r.e          = e;
        r.popup_xy   = wt.position;
        r.obs        = obs;
        r.has_timing = false;
        if (hit_buf.size() >= hit_buf.capacity()) {
            ++scratch.hit_capacity_exceeded_count;
        }
        hit_buf.push_back(r);
    }

    if (hit_buf.empty()) return;

    for (auto& r : hit_buf) {
        const bool contributes_to_chain = r.obs.base_points > 0;
        if (contributes_to_chain) {
            score.chain_count++;
        }
        const float chain_mult = chain_multiplier_for_count(score.chain_count);
        // Ungraded hits never had a tier — pre-migration the switch fell
        // through with timing_mult = 1.0f. Preserve that exactly.
        int points = static_cast<int>(
            std::floor(static_cast<float>(r.obs.base_points) * chain_mult));

        if (contributes_to_chain && results && score.chain_count > results->max_chain) {
            results->max_chain = score.chain_count;
        }

        score.score += points;

        if (popup_queue.untimed.size() >= popup_queue.untimed.capacity()) {
            ++popup_queue.capacity_exceeded_count;
        }
        popup_queue.untimed.push_back({r.popup_xy.x, r.popup_xy.y, points});
        spawn_score_particles(reg, r.popup_xy, kUntimedParticleColor);

        reg.get_or_emplace<ResolvedObstacleTag>(r.e);
        reg.remove<Obstacle>(r.e);
        reg.remove<ScoredTag>(r.e);
    }
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

    // Hoist single scratch lookup — miss_buf and hit_buf share the same struct.
    auto& scratch     = reg.ctx().get<ScoringSystemScratch>();
    auto& popup_queue = reg.ctx().get<ScorePopupRequestQueue>();

    // Miss pass: single owner of ENERGY_DRAIN_MISS and miss_count.
    // Collect first, then remove view components after iteration (#315).
    {
        auto& miss_buf = scratch.miss_buf;
        miss_buf.clear();

        auto miss_view_graded = reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle, TimingGrade>(
            entt::exclude<NonScorableTag>);
        for (auto e : miss_view_graded) {
            enqueue_energy_effect(reg, -constants::ENERGY_DRAIN_MISS, true);
            if (results) results->miss_count++;
            score.chain_count = 0;
            if (miss_buf.size() >= miss_buf.capacity()) {
                ++scratch.miss_capacity_exceeded_count;
            }
            miss_buf.push_back({e, true});
        }

        auto miss_view_ungraded = reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle>(
            entt::exclude<TimingGrade, NonScorableTag>);
        for (auto e : miss_view_ungraded) {
            enqueue_energy_effect(reg, -constants::ENERGY_DRAIN_MISS, true);
            if (results) results->miss_count++;
            score.chain_count = 0;
            if (miss_buf.size() >= miss_buf.capacity()) {
                ++scratch.miss_capacity_exceeded_count;
            }
            miss_buf.push_back({e, false});
        }
        // Apply structural removals after iteration — safe.
        for (auto& r : miss_buf) {
            reg.get_or_emplace<ResolvedObstacleTag>(r.e);
            reg.remove<Obstacle>(r.e);
            reg.remove<ScoredTag>(r.e);
            if (reg.all_of<MissTag>(r.e)) reg.remove<MissTag>(r.e);
            if (r.has_timing) remove_timing_grade_and_tags(reg, r.e);
        }
    }

    // Hit pass: one transform per former TimingTier value plus one ungraded
    // transform (#1202/#1204). Per-tier constants live in tier_score_traits;
    // no `switch` on a discriminator anywhere in the hit pipeline.
    process_tier_hit_pass<TimingPerfectTag>(reg, score, results, scratch, popup_queue);
    process_tier_hit_pass<TimingGoodTag>   (reg, score, results, scratch, popup_queue);
    process_tier_hit_pass<TimingOkTag>     (reg, score, results, scratch, popup_queue);
    process_tier_hit_pass<TimingBadTag>    (reg, score, results, scratch, popup_queue);
    process_ungraded_hit_pass              (reg, score, results, scratch, popup_queue);

    // ── NonScorable cleanup ───────────────────────────────────────────────────
    // Entities excluded from the hit pass via NonScorableTag still need their
    // ScoredTag and Obstacle stripped after collision resolution so they are not
    // re-processed on the next frame. Collect-then-remove follows the same EnTT
    // safety pattern as the hit/miss passes above. (#315)
    {
        // Re-use hit_buf (already cleared above) as a scratch collect buffer.
        auto& cleanup_buf = scratch.hit_buf;
        cleanup_buf.clear();

        auto ns_view = reg.view<ObstacleTag, ScoredTag, NonScorableTag>();
        for (auto e : ns_view) {
            HitRecord r;
            r.e = e;
            r.has_timing = reg.all_of<TimingGrade>(e);
            if (cleanup_buf.size() >= cleanup_buf.capacity()) {
                ++scratch.hit_capacity_exceeded_count;
            }
            cleanup_buf.push_back(r);
        }
        for (auto& r : cleanup_buf) {
            reg.get_or_emplace<ResolvedObstacleTag>(r.e);
            reg.remove<Obstacle>(r.e);
            reg.remove<ScoredTag>(r.e);
            if (reg.all_of<MissTag>(r.e)) reg.remove<MissTag>(r.e);
            if (r.has_timing) remove_timing_grade_and_tags(reg, r.e);
        }
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
