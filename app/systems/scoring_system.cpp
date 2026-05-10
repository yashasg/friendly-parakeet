#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/obstacle.h"
#include "../components/gameplay_intents.h"
#include "../components/system_scratch.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/haptics.h"
#include "../util/settings.h"
#include "../components/rhythm.h"
#include "../util/rhythm_math.h"
#include "../constants.h"
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
    pending.events.push_back(PendingEnergyEffects::Event{delta, flash});
}

ScorePopupRequestQueue& popup_queue_for(entt::registry& reg) {
    return reg.ctx().get<ScorePopupRequestQueue>();
}

}  // namespace

void scoring_system(entt::registry& reg, float dt) {
    auto& score   = reg.ctx().get<ScoreState>();
    auto* song    = reg.ctx().find<SongState>();
    const float scroll_speed = song ? song->scroll_speed : constants::BASE_SCROLL_SPEED;

    // Passive score accrual (distance/time) only while song playback is active.
    const bool allow_passive_score = (song == nullptr) || !song->finished;
    if (allow_passive_score) {
        score.distance_traveled += scroll_speed * dt;
        score.score += static_cast<int>(dt * constants::PTS_PER_SECOND);
    }

    // Chain timer
    score.chain_timer += dt;
    if (score.chain_timer > 2.0f) {
        score.chain_count = 0;
    }

    auto* results = reg.ctx().find<SongResults>();   // #309: hoisted above loop

    // Hoist single scratch lookup — miss_buf and hit_buf share the same struct.
    auto& scratch = scoring_scratch_for(reg);

    // ── Miss pass ────────────────────────────────────────────────────────────
    // Structural view: only entities carrying MissTag.
    // Single owner of ENERGY_DRAIN_MISS and miss_count for all miss paths
    // (collision miss and scroll-past miss both route through here).
    //
    // EnTT safety: Obstacle and ScoredTag are view components; removing them
    // mid-iteration would swap-and-pop the pool (UB). Collect entities during
    // the read pass, apply removals after the view is exhausted. (#315)
    {
        auto& miss_buf = scratch.miss_buf;
        miss_buf.clear();

        auto miss_view_graded = reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle, TimingGrade>();
        for (auto e : miss_view_graded) {
            enqueue_energy_effect(reg, -constants::ENERGY_DRAIN_MISS, true);
            if (results) results->miss_count++;
            score.chain_count = 0;
            score.chain_timer = 0.0f;
            miss_buf.push_back({e, true});
        }

        auto miss_view_ungraded = reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle>(
            entt::exclude<TimingGrade>);
        for (auto e : miss_view_ungraded) {
            enqueue_energy_effect(reg, -constants::ENERGY_DRAIN_MISS, true);
            if (results) results->miss_count++;
            score.chain_count = 0;
            score.chain_timer = 0.0f;
            miss_buf.push_back({e, false});
        }
        // Apply structural removals after iteration — safe.
        for (auto& r : miss_buf) {
            reg.remove<Obstacle>(r.e);
            reg.remove<ScoredTag>(r.e);
            reg.remove<MissTag>(r.e);
            if (r.has_timing) reg.remove<TimingGrade>(r.e);
        }
    }

    // ── Hit pass ─────────────────────────────────────────────────────────────
    // Structural view: ScoredTag without MissTag. Collect all data needed for
    // popup creation and scoring, then process and remove after iteration.
    //
    // EnTT safety: same collect-then-remove pattern as miss pass. (#315)
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

            int points = static_cast<int>(
                std::floor(r.obs.base_points * timing_mult));

            // Chain bonus
            score.chain_count++;
            score.chain_timer = 0.0f;
            if (score.chain_count >= 2 && score.chain_count <= 4) {
                points += constants::CHAIN_BONUS[score.chain_count];
            } else if (score.chain_count >= 5) {
                points += constants::CHAIN_BONUS[4] + (score.chain_count - 4) * 100;
            }

            if (results && score.chain_count > results->max_chain) {
                results->max_chain = score.chain_count;
            }

            score.score += points;

            // Queue timing/score popup; popup_feedback_system owns spawn/SFX.
            std::optional<TimingTier> tt = r.has_timing
                ? std::make_optional(r.timing.tier) : std::nullopt;
            popup_queue.requests.push_back({r.popup_xy.x, r.popup_xy.y, points, tt});

            // Structural removals after all reads — safe.
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

        auto ns_view = reg.view<ObstacleTag, ScoredTag, NonScorableTag>(
            entt::exclude<MissTag>);
        for (auto e : ns_view) {
            HitRecord r;
            r.e = e;
            cleanup_buf.push_back(r);
        }
        for (auto& r : cleanup_buf) {
            reg.remove<Obstacle>(r.e);
            reg.remove<ScoredTag>(r.e);
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
