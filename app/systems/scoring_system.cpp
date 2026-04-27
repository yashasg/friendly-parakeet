#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/lifetime.h"
#include "../components/audio.h"
#include "../components/haptics.h"
#include "../components/settings.h"
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

    // Process scored obstacles
    auto view = reg.view<ObstacleTag, ScoredTag, Obstacle, Position>();
    auto* energy = reg.ctx().find<EnergyState>();
    for (auto [entity, obs, pos] : view.each()) {
        if (reg.any_of<MissTag>(entity)) {
            // Single owner of ENERGY_DRAIN_MISS and miss_count for all miss paths
            // (collision miss and scroll-past miss both route through here).
            if (energy) {
                energy->energy -= constants::ENERGY_DRAIN_MISS;
                if (energy->energy < 1e-6f) energy->energy = 0.0f;
                energy->flash_timer = constants::ENERGY_FLASH_DURATION;
            }
            if (auto* results = reg.ctx().find<SongResults>()) {
                results->miss_count++;
            }
            score.chain_count = 0;
            score.chain_timer = 0.0f;
            reg.remove<Obstacle>(entity);
            reg.remove<ScoredTag>(entity);
            reg.remove<MissTag>(entity);
            if (reg.any_of<TimingGrade>(entity))  reg.remove<TimingGrade>(entity);
            continue;
        }

        // LanePush is passive scenery — excluded from the burnout ladder:
        // no score popup, no chain contribution, no best_burnout update.
        if (obs.kind == ObstacleKind::LanePushLeft ||
            obs.kind == ObstacleKind::LanePushRight) {
            reg.remove<Obstacle>(entity);
            reg.remove<ScoredTag>(entity);
            continue;
        }

        // Scoring uses a flat 1.0× base — burnout multipliers removed (#239).
        float burnout_mult = 1.0f;

        // Check for timing grade (rhythm mode)
        float timing_mult = 1.0f;
        auto* timing = reg.try_get<TimingGrade>(entity);
        if (timing) {
            timing_mult = timing_multiplier(timing->tier);
        }

        // Energy adjustment based on timing
        if (timing) {
            if (energy) {
                switch (timing->tier) {
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
        }

        int points = static_cast<int>(std::floor(obs.base_points * timing_mult * burnout_mult));

        // Chain bonus
        score.chain_count++;
        score.chain_timer = 0.0f;
        if (score.chain_count >= 2 && score.chain_count <= 4) {
            points += constants::CHAIN_BONUS[score.chain_count];
        } else if (score.chain_count >= 5) {
            points += constants::CHAIN_BONUS[4] + (score.chain_count - 4) * 100;
        }

        // Update max chain in results
        auto* results = reg.ctx().find<SongResults>();
        if (results) {
            if (score.chain_count > results->max_chain) {
                results->max_chain = score.chain_count;
            }
        }

        score.score += points;

        // Spawn timing/score popup
        auto popup = reg.create();
        reg.emplace<Position>(popup, pos.x, pos.y - 40.0f);
        reg.emplace<Velocity>(popup, 0.0f, -80.0f);
        reg.emplace<Lifetime>(popup, constants::POPUP_DURATION, constants::POPUP_DURATION);

        std::optional<TimingTier> tt = timing ? std::make_optional(timing->tier) : std::nullopt;
        reg.emplace<ScorePopup>(popup, points, uint8_t{0}, tt);

        // Color by timing grade
        uint8_t pr = 255, pg = 255, pb = 50;
        if (timing) {
            switch (timing->tier) {
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
        init_popup_display(pd, reg.get<ScorePopup>(popup),
                           Color{pr, pg, pb, 255});
        reg.emplace<PopupDisplay>(popup, pd);

        audio_push(reg.ctx().get<AudioQueue>(), SFX::ScorePopup);

        reg.remove<Obstacle>(entity);
        reg.remove<ScoredTag>(entity);
        if (timing) reg.remove<TimingGrade>(entity);
    }

    // Smooth score display
    float display_speed = 5000.0f * dt;
    if (score.displayed_score < score.score) {
        score.displayed_score += static_cast<int>(display_speed);
        if (score.displayed_score > score.score)
            score.displayed_score = score.score;
    }
}
