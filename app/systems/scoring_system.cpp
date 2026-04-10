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

        // Spawn score popup
        auto popup = reg.create();
        reg.emplace<Position>(popup, pos.x, pos.y - 40.0f);
        reg.emplace<Velocity>(popup, 0.0f, -80.0f);
        reg.emplace<Lifetime>(popup, constants::POPUP_DURATION, constants::POPUP_DURATION);
        reg.emplace<ScorePopup>(popup, points, tier_for_multiplier(burnout_mult));
        reg.emplace<DrawColor>(popup, uint8_t{255}, uint8_t{255}, uint8_t{50}, uint8_t{255});
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
