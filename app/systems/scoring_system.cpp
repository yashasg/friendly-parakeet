#include "all_systems.h"
#include "../entities/popup_entity.h"
#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../audio/audio_queue.h"
#include "../components/haptics.h"
#include "../util/settings.h"
#include "../components/rhythm.h"
#include "../util/rhythm_math.h"
#include "../constants.h"
#include <cmath>
#include <vector>

namespace {

struct MissRecord {
    entt::entity e;
    bool has_timing = false;
};

struct HitRecord {
    entt::entity e;
    Position     pos;
    Obstacle     obs;
    bool         has_timing = false;
    TimingGrade  timing{};
};

struct ScoringSystemScratch {
    std::vector<MissRecord> miss_buf;
    std::vector<HitRecord> hit_buf;
};

ScoringSystemScratch& scoring_scratch_for(entt::registry& reg) {
    if (auto* scratch = reg.ctx().find<ScoringSystemScratch>()) {
        return *scratch;
    }
    return reg.ctx().emplace<ScoringSystemScratch>();
}

}  // namespace

void scoring_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto& score   = reg.ctx().get<ScoreState>();
    auto* song    = reg.ctx().find<SongState>();
    const float scroll_speed = song ? song->scroll_speed : constants::BASE_SCROLL_SPEED;

    // Distance bonus
    score.distance_traveled += scroll_speed * dt;
    score.score += static_cast<int>(dt * constants::PTS_PER_SECOND);

    // Chain timer
    score.chain_timer += dt;
    if (score.chain_timer > 2.0f) {
        score.chain_count = 0;
    }

    auto* energy  = reg.ctx().find<EnergyState>();   // #309: hoisted above loop
    auto* results = reg.ctx().find<SongResults>();   // #309: hoisted above loop

    // ── Miss pass ────────────────────────────────────────────────────────────
    // Structural view: only entities carrying MissTag.
    // Single owner of ENERGY_DRAIN_MISS and miss_count for all miss paths
    // (collision miss and scroll-past miss both route through here).
    //
    // EnTT safety: Obstacle and ScoredTag are view components; removing them
    // mid-iteration would swap-and-pop the pool (UB). Collect entities during
    // the read pass, apply removals after the view is exhausted. (#315)
    {
        auto& miss_buf = scoring_scratch_for(reg).miss_buf;
        miss_buf.clear();

        auto miss_view = reg.view<ObstacleTag, ScoredTag, MissTag, Obstacle>();
        for (auto e : miss_view) {
            if (energy) {
                energy->energy -= constants::ENERGY_DRAIN_MISS;
                if (energy->energy < 1e-6f) energy->energy = 0.0f;
                energy->flash_timer = constants::ENERGY_FLASH_DURATION;
            }
            if (results) results->miss_count++;
            score.chain_count = 0;
            score.chain_timer = 0.0f;
            miss_buf.push_back({e, reg.any_of<TimingGrade>(e)});
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
        auto& hit_buf = scoring_scratch_for(reg).hit_buf;
        hit_buf.clear();

        auto hit_view = reg.view<ObstacleTag, ScoredTag, Obstacle, Position>(
            entt::exclude<MissTag>);
        for (auto [e, obs, pos] : hit_view.each()) {
            HitRecord r;
            r.e   = e;
            r.pos = pos;
            r.obs = obs;
            auto* tg = reg.try_get<TimingGrade>(e);
            if (tg) { r.has_timing = true; r.timing = *tg; }
            hit_buf.push_back(r);
        }

        auto model_hit_view = reg.view<ObstacleTag, ScoredTag, Obstacle, ObstacleScrollZ>(
            entt::exclude<MissTag, Position>);
        for (auto [e, obs, oz] : model_hit_view.each()) {
            HitRecord r;
            r.e   = e;
            r.pos = Position{constants::SCREEN_W_F * 0.5f, oz.z};
            r.obs = obs;
            auto* tg = reg.try_get<TimingGrade>(e);
            if (tg) { r.has_timing = true; r.timing = *tg; }
            hit_buf.push_back(r);
        }

        for (auto& r : hit_buf) {
            // LanePush is passive scenery — excluded from the scoring ladder:
            // no score popup, no chain contribution.
            if (r.obs.kind == ObstacleKind::LanePushLeft ||
                r.obs.kind == ObstacleKind::LanePushRight) {
                reg.remove<Obstacle>(r.e);
                reg.remove<ScoredTag>(r.e);
                continue;
            }

            float timing_mult  = r.has_timing ? timing_multiplier(r.timing.tier) : 1.0f;

            // Energy adjustment based on timing
            if (r.has_timing && energy) {
                switch (r.timing.tier) {
                    case TimingTier::Perfect:
                        energy->energy += constants::ENERGY_RECOVER_PERFECT;
                        break;
                    case TimingTier::Good:
                        energy->energy += constants::ENERGY_RECOVER_GOOD;
                        break;
                    case TimingTier::Ok:
                        energy->energy += constants::ENERGY_RECOVER_OK;
                        break;
                    case TimingTier::Bad:
                        energy->energy -= constants::ENERGY_DRAIN_BAD;
                        energy->flash_timer = constants::ENERGY_FLASH_DURATION;
                        break;
                }
                if (energy->energy < 0.0f) energy->energy = 0.0f;
                if (energy->energy > constants::ENERGY_MAX) energy->energy = constants::ENERGY_MAX;
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

            // Spawn timing/score popup via entity factory (#349).
            std::optional<TimingTier> tt = r.has_timing
                ? std::make_optional(r.timing.tier) : std::nullopt;
            spawn_score_popup(reg, {r.pos.x, r.pos.y, points, tt});
            audio_push(reg.ctx().get<AudioQueue>(), SFX::ScorePopup);

            // Structural removals after all reads — safe.
            reg.remove<Obstacle>(r.e);
            reg.remove<ScoredTag>(r.e);
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
