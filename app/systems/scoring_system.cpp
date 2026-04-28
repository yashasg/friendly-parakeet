#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/lifetime.h"
#include "audio_types.h"
#include "../components/haptics.h"
#include "../util/settings.h"
#include "../components/difficulty.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <cmath>

void scoring_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto& score   = reg.ctx().get<ScoreState>();
    auto& config  = reg.ctx().get<DifficultyConfig>();

    // Distance bonus
    score.distance_traveled += config.scroll_speed * dt;
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
        struct MissRecord { entt::entity e; bool has_timing; };
        static std::vector<MissRecord> miss_buf;
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
        struct HitRecord {
            entt::entity e;
            Position     pos;
            Obstacle     obs;
            bool         has_timing = false;
            TimingGrade  timing{};
        };
        static std::vector<HitRecord> hit_buf;
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

            // Spawn timing/score popup
            auto popup = reg.create();
            reg.emplace<Position>(popup, r.pos.x, r.pos.y - 40.0f);
            reg.emplace<Velocity>(popup, 0.0f, -80.0f);
            reg.emplace<Lifetime>(popup, constants::POPUP_DURATION, constants::POPUP_DURATION);

            std::optional<TimingTier> tt = r.has_timing
                ? std::make_optional(r.timing.tier) : std::nullopt;
            reg.emplace<ScorePopup>(popup, points, uint8_t{0}, tt);

            // Color by timing grade
            uint8_t pr = 255, pg = 255, pb = 50;
            if (r.has_timing) {
                switch (r.timing.tier) {
                    case TimingTier::Perfect: pr = 100; pg = 255; pb = 100; break; // green
                    case TimingTier::Good:    pr = 180; pg = 255; pb = 100; break; // yellow-green
                    case TimingTier::Ok:      pr = 255; pg = 255; pb = 100; break; // yellow
                    case TimingTier::Bad:     pr = 255; pg = 150; pb = 100; break; // orange
                }
            }
            reg.emplace<Color>(popup, Color{pr, pg, pb, 255});
            reg.emplace<DrawLayer>(popup, Layer::Effects);

            // Format the popup display once at spawn (#251): popup_display_system
            // only updates the alpha each frame; text/font/base RGB stay put.
            PopupDisplay pd{};
            init_popup_display(pd, reg.get<ScorePopup>(popup), Color{pr, pg, pb, 255});
            reg.emplace<PopupDisplay>(popup, pd);

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
