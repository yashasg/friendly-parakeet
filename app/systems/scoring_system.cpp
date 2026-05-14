#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/obstacle.h"
#include "../components/gameplay_intents.h"
#include "../components/particle.h"
#include "../components/system_scratch.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/rhythm.h"
#include "../util/rhythm_math.h"
#include "../constants.h"
#include <algorithm>
#include <cmath>

namespace {

ScoringSystemScratch& scoring_scratch_for(entt::registry& reg) {
    return reg.ctx().get<ScoringSystemScratch>();
}

PendingEnergyEffects& pending_energy_for(entt::registry& reg) {
    return reg.ctx().get<PendingEnergyEffects>();
}

void enqueue_energy_effect(entt::registry& reg, float delta, bool flash = false) {
    auto& pending = pending_energy_for(reg);
    if (pending.events.size() >= pending.events.capacity()) {
        ++pending.capacity_exceeded_count;
    }
    pending.events.push_back(PendingEnergyEffects::Event{delta, flash});
}

ScorePopupRequestQueue& popup_queue_for(entt::registry& reg) {
    return reg.ctx().get<ScorePopupRequestQueue>();
}

Color score_particle_color(const HitRecord& record) {
    if (!record.has_timing) {
        return Color{220, 220, 255, 220};
    }

    switch (record.timing.tier) {
        case TimingTier::Perfect:
            return Color{255, 230, 90, 240};
        case TimingTier::Good:
            return Color{120, 255, 160, 230};
        case TimingTier::Ok:
            return Color{100, 180, 255, 220};
        case TimingTier::Bad:
            return Color{255, 100, 100, 220};
    }

    return Color{220, 220, 255, 220};
}

void spawn_score_particles(entt::registry& reg, const glm::vec2& position, Color color) {
    constexpr float kLifetime = 0.35f;
    constexpr float kSize = 10.0f;
    constexpr float kSpeed = 90.0f;
    constexpr glm::vec2 kDirections[] = {
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
        reg.emplace<WorldTransform>(particle, WorldTransform{position});
        reg.emplace<MotionVelocity>(particle, MotionVelocity{dir * kSpeed});
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

}  // namespace

void scoring_system(entt::registry& reg, float dt) {
    auto& score   = reg.ctx().get<ScoreState>();
    auto* song    = reg.ctx().find<SongState>();

    // Passive score accrual (PTS_PER_SECOND) only while song playback is active.
    const bool allow_passive_score = (song == nullptr) || !song->finished;
    if (allow_passive_score) {
        const float passive_score = score.passive_score_remainder +
            dt * static_cast<float>(constants::PTS_PER_SECOND);
        const int whole_points = static_cast<int>(std::floor(passive_score));
        score.score += whole_points;
        score.passive_score_remainder = passive_score - static_cast<float>(whole_points);
    }

    auto* results = reg.ctx().find<SongResults>();   // #309: hoisted above loop

    // Hoist single scratch lookup — miss_buf and hit_buf share the same struct.
    auto& scratch = scoring_scratch_for(reg);

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
            if (r.has_timing) reg.remove<TimingGrade>(r.e);
        }
    }

    // Hit pass: collect popup/scoring data, then process and remove (#315).
    {
        auto& hit_buf = scratch.hit_buf;
        hit_buf.clear();

        auto hit_view_graded = reg.view<ObstacleTag, ScoredTag, Obstacle, WorldTransform, TimingGrade>(
            entt::exclude<MissTag, NonScorableTag>);
        for (auto [e, obs, wt, tg] : hit_view_graded.each()) {
            HitRecord r;
            r.e        = e;
            r.popup_xy = wt.position;
            r.obs      = obs;
            r.has_timing = true;
            r.timing = tg;
            if (hit_buf.size() >= hit_buf.capacity()) {
                ++scratch.hit_capacity_exceeded_count;
            }
            hit_buf.push_back(r);
        }

        auto hit_view_ungraded = reg.view<ObstacleTag, ScoredTag, Obstacle, WorldTransform>(
            entt::exclude<MissTag, NonScorableTag, TimingGrade>);
        for (auto [e, obs, wt] : hit_view_ungraded.each()) {
            HitRecord r;
            r.e        = e;
            r.popup_xy = wt.position;
            r.obs      = obs;
            r.has_timing = false;
            if (hit_buf.size() >= hit_buf.capacity()) {
                ++scratch.hit_capacity_exceeded_count;
            }
            hit_buf.push_back(r);
        }


        if (!hit_buf.empty()) {
        auto& popup_queue = popup_queue_for(reg);
        for (auto& r : hit_buf) {
            float timing_mult  = r.has_timing ? timing_multiplier(r.timing.tier) : 1.0f;

            // Energy adjustment based on timing
            if (r.has_timing) {
                switch (r.timing.tier) {
                    case TimingTier::Perfect:
                        enqueue_energy_effect(reg, constants::ENERGY_RECOVER_PERFECT);
                        break;
                    case TimingTier::Good:
                        enqueue_energy_effect(reg, constants::ENERGY_RECOVER_GOOD);
                        break;
                    case TimingTier::Ok:
                        enqueue_energy_effect(reg, constants::ENERGY_RECOVER_OK);
                        break;
                    case TimingTier::Bad:
                        enqueue_energy_effect(reg, -constants::ENERGY_DRAIN_BAD, true);
                        break;
                }
                if (results) {
                    switch (r.timing.tier) {
                        case TimingTier::Perfect: results->perfect_count++; break;
                        case TimingTier::Good:    results->good_count++;    break;
                        case TimingTier::Ok:      results->ok_count++;      break;
                        case TimingTier::Bad:     results->bad_count++;     break;
                    }
                }
            }

            const bool contributes_to_chain = r.obs.base_points > 0;
            if (contributes_to_chain) {
                score.chain_count++;
            }
            const float chain_mult = chain_multiplier_for_count(score.chain_count);
            int points = static_cast<int>(
                std::floor(static_cast<float>(r.obs.base_points) * timing_mult * chain_mult));

            if (contributes_to_chain && results && score.chain_count > results->max_chain) {
                results->max_chain = score.chain_count;
            }

            score.score += points;

            // Queue timing/score popup; popup_feedback_system owns spawn/SFX.
            if (popup_queue.requests.size() >= popup_queue.requests.capacity()) {
                ++popup_queue.capacity_exceeded_count;
            }
            popup_queue.requests.push_back({
                r.popup_xy.x,
                r.popup_xy.y,
                points,
                r.has_timing,
                r.has_timing ? r.timing.tier : TimingTier::Ok,
            });
            spawn_score_particles(reg, r.popup_xy, score_particle_color(r));

            // Structural removals after all reads — safe.
            reg.get_or_emplace<ResolvedObstacleTag>(r.e);
            reg.remove<Obstacle>(r.e);
            reg.remove<ScoredTag>(r.e);
            if (r.has_timing) reg.remove<TimingGrade>(r.e);
        }
        } // !hit_buf.empty()
    }

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
            if (r.has_timing) reg.remove<TimingGrade>(r.e);
        }
    }

    // Smooth score display
    float display_speed = 5000.0f * dt;
    if (score.displayed_score < score.score) {
        score.displayed_score += static_cast<int>(display_speed);
        if (score.displayed_score > score.score)
            score.displayed_score = score.score;
    }
}
