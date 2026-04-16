#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/scoring.h"
#include "../components/burnout.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/lifetime.h"
#include "../components/audio.h"
#include "../components/difficulty.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <cmath>

static float multiplier_for_zone(BurnoutZone zone) {
    switch (zone) {
        case BurnoutZone::None:   return constants::MULT_SAFE;
        case BurnoutZone::Safe:   return constants::MULT_SAFE;
        case BurnoutZone::Risky:  return constants::MULT_RISKY;
        case BurnoutZone::Danger: return constants::MULT_DANGER;
        case BurnoutZone::Dead:   return constants::MULT_CLUTCH;
    }
    return 1.0f;
}

static uint8_t tier_for_multiplier(float mult) {
    if (mult >= 5.0f) return 5;
    if (mult >= 4.0f) return 4;
    if (mult >= 3.0f) return 3;
    if (mult >= 2.0f) return 2;
    if (mult >= 1.5f) return 1;
    return 0;
}

void scoring_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto& score   = reg.ctx().get<ScoreState>();
    auto& burnout = reg.ctx().get<BurnoutState>();
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
    for (auto [entity, obs, pos] : view.each()) {
        float burnout_mult = multiplier_for_zone(burnout.zone);

        // Check for timing grade (rhythm mode)
        float timing_mult = 1.0f;
        auto* timing = reg.try_get<TimingGrade>(entity);
        if (timing) {
            timing_mult = timing_multiplier(timing->tier);
        }

        // Energy adjustment based on timing
        if (timing) {
            auto* energy = reg.ctx().find<EnergyState>();
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
            if (burnout_mult > results->best_burnout) {
                results->best_burnout = burnout_mult;
            }
        }

        score.score += points;

        // Spawn timing/score popup
        auto popup = reg.create();
        reg.emplace<Position>(popup, pos.x, pos.y - 40.0f);
        reg.emplace<Velocity>(popup, 0.0f, -80.0f);
        reg.emplace<Lifetime>(popup, constants::POPUP_DURATION, constants::POPUP_DURATION);

        uint8_t tt = timing ? static_cast<uint8_t>(timing->tier) : uint8_t{255};
        reg.emplace<ScorePopup>(popup, points, tier_for_multiplier(burnout_mult), tt);

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
        reg.emplace<DrawColor>(popup, pr, pg, pb, uint8_t{255});
        reg.emplace<DrawLayer>(popup, Layer::Effects);

        audio_push(reg.ctx().get<AudioQueue>(), SFX::BurnoutBank);

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
