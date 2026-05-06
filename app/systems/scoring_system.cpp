#include "../components/scoring.h"
#include "../components/obstacle.h"
#include "../components/gameplay_intents.h"
#include "../components/registry_context.h"
#include "../components/transform.h"
#include <glm/vec2.hpp>
#include "../components/rhythm.h"
#include "../util/rhythm_math.h"
#include "../constants.h"
#include <entt/entt.hpp>
#include <cmath>
#include <vector>

namespace {

struct MissRecord {
    entt::entity e;
    bool has_timing = false;
};

struct HitRecord {
    entt::entity e;
    glm::vec2    popup_xy = {0.0f, 0.0f};
    Obstacle     obs;
    bool         has_timing = false;
    TimingGrade  timing{};
};

struct ScoringSystemScratch {
    std::vector<MissRecord> miss_buf;
    std::vector<HitRecord>  hit_buf;
    std::vector<entt::entity> cleanup_buf;
};

void enqueue_energy_effect(entt::registry& reg, float delta, bool flash = false);

void register_miss(entt::registry& reg,
                   entt::entity entity,
                   ScoreState& score,
                   SongResults* results,
                   GameOverState* gos,
                   std::vector<MissRecord>& miss_buf,
                   bool has_timing) {
    enqueue_energy_effect(reg, -constants::ENERGY_DRAIN_MISS, true);
    if (results) {
        results->miss_count++;
    }
    score.chain_count = 0;
    score.chain_timer = 0.0f;
    if (gos && gos->cause == DeathCause::None) {
        gos->cause = reg.any_of<BarObstacleTag>(entity) ? DeathCause::HitABar : DeathCause::MissedABeat;
    }
    miss_buf.push_back({entity, has_timing});
}

void push_hit_record(std::vector<HitRecord>& hit_buf,
                     entt::entity entity,
                     const Obstacle& obstacle,
                     glm::vec2 popup_xy,
                     const TimingGrade* timing) {
    HitRecord record{};
    record.e = entity;
    record.obs = obstacle;
    record.popup_xy = popup_xy;
    record.has_timing = (timing != nullptr);
    if (timing) {
        record.timing = *timing;
    }
    hit_buf.push_back(record);
}

void enqueue_energy_effect(entt::registry& reg, float delta, bool flash) {
    auto& pending = registry_ctx_get_or_emplace<PendingEnergyEffects>(reg);
    pending.events.push_back(PendingEnergyEffects::Event{delta, flash});
}

}  // namespace

void scoring_system(entt::registry& reg, float dt) {
    auto& score   = registry_ctx_get<ScoreState>(reg);
    auto* song    = registry_ctx_find<SongState>(reg);
    const float scroll_speed = song ? song->scroll_speed : constants::BASE_SCROLL_SPEED;

    // Distance bonus
    score.distance_traveled += scroll_speed * dt;
    score.score += static_cast<int>(dt * constants::PTS_PER_SECOND);

    // Chain timer
    score.chain_timer += dt;
    if (score.chain_timer > 2.0f) {
        score.chain_count = 0;
    }

    auto* results = registry_ctx_find<SongResults>(reg);   // #309: hoisted above loop
    auto* gos     = registry_ctx_find<GameOverState>(reg);

    // Hoist single scratch lookup — miss_buf and hit_buf share the same struct.
    auto& scratch = registry_ctx_get_or_emplace<ScoringSystemScratch>(reg);

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
            register_miss(reg, e, score, results, gos, miss_buf, true);
        }

        auto miss_view_ungraded = reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle>(
            entt::exclude<TimingGrade>);
        for (auto e : miss_view_ungraded) {
            register_miss(reg, e, score, results, gos, miss_buf, false);
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
            entt::exclude<MissTag, NonScorableTag, ObstacleScrollZ>);
        for (auto [e, obs, wt, tg] : hit_view_graded.each()) {
            push_hit_record(hit_buf, e, obs, wt.position, &tg);
        }

        auto hit_view_ungraded = reg.view<ObstacleTag, ScoredTag, Obstacle, WorldTransform>(
            entt::exclude<MissTag, NonScorableTag, ObstacleScrollZ, TimingGrade>);
        for (auto [e, obs, wt] : hit_view_ungraded.each()) {
            push_hit_record(hit_buf, e, obs, wt.position, nullptr);
        }

        auto model_hit_view_graded = reg.view<ObstacleTag, ScoredTag, Obstacle, ObstacleScrollZ, TimingGrade>(
            entt::exclude<MissTag, NonScorableTag>);
        for (auto [e, obs, oz, tg] : model_hit_view_graded.each()) {
            push_hit_record(hit_buf, e, obs, {constants::SCREEN_W_F * 0.5f, oz.z}, &tg);
        }

        auto model_hit_view_ungraded = reg.view<ObstacleTag, ScoredTag, Obstacle, ObstacleScrollZ>(
            entt::exclude<MissTag, NonScorableTag, TimingGrade>);
        for (auto [e, obs, oz] : model_hit_view_ungraded.each()) {
            push_hit_record(hit_buf, e, obs, {constants::SCREEN_W_F * 0.5f, oz.z}, nullptr);
        }

        if (!hit_buf.empty()) {
            auto& popup_queue = registry_ctx_get_or_emplace<ScorePopupRequestQueue>(reg);
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
        auto& cleanup_buf = scratch.cleanup_buf;
        cleanup_buf.clear();

        auto ns_view = reg.view<ObstacleTag, ScoredTag, NonScorableTag>(
            entt::exclude<MissTag>);
        for (auto e : ns_view) {
            cleanup_buf.push_back(e);
        }
        for (const auto e : cleanup_buf) {
            reg.remove<Obstacle>(e);
            reg.remove<ScoredTag>(e);
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
