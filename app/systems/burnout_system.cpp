#include "all_systems.h"
#include "../components/burnout.h"
#include "../components/game_state.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/difficulty.h"
#include "../components/audio.h"
#include "../constants.h"
#include <cmath>
#include <limits>

void burnout_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto& burnout = reg.ctx().get<BurnoutState>();
    auto& config  = reg.ctx().get<DifficultyConfig>();

    // Find player
    auto player_view = reg.view<PlayerTag, Position, Lane>();
    if (player_view.size_hint() == 0) {
        burnout.nearest_threat = entt::null;
        return;
    }

    auto player_it = player_view.begin();
    auto [p_pos, p_lane] = player_view.get<Position, Lane>(*player_it);

    // Find nearest un-scored obstacle above player
    float nearest_dist = std::numeric_limits<float>::max();
    entt::entity nearest = entt::null;

    auto obs_view = reg.view<ObstacleTag, Position, Obstacle>(entt::exclude<ScoredTag>);
    for (auto [entity, obs_pos, obs] : obs_view.each()) {
        float dist = p_pos.y - obs_pos.y;
        if (dist > 0.0f && dist < nearest_dist) {
            nearest_dist = dist;
            nearest = entity;
        }
    }

    burnout.nearest_threat  = nearest;
    burnout.threat_distance = nearest_dist;

    if (nearest == entt::null) {
        burnout.meter = 0.0f;
        burnout.zone  = BurnoutZone::None;
        return;
    }

    // Scale zones by difficulty
    float scale = config.burnout_window_scale;
    float safe_max   = constants::ZONE_SAFE_MAX * scale;
    float safe_min   = constants::ZONE_SAFE_MIN * scale;
    float risky_min  = constants::ZONE_RISKY_MIN * scale;
    float danger_min = constants::ZONE_DANGER_MIN * scale;

    BurnoutZone prev_zone = burnout.zone;

    if (nearest_dist > safe_max) {
        burnout.zone  = BurnoutZone::None;
        burnout.meter = 0.0f;
    } else if (nearest_dist > safe_min) {
        burnout.zone  = BurnoutZone::Safe;
        burnout.meter = 1.0f - (nearest_dist - safe_min) / (safe_max - safe_min);
        burnout.meter *= 0.4f; // Safe zone = 0..0.4
    } else if (nearest_dist > risky_min) {
        burnout.zone  = BurnoutZone::Risky;
        burnout.meter = 0.4f + (1.0f - (nearest_dist - risky_min) / (safe_min - risky_min)) * 0.3f;
    } else if (nearest_dist > danger_min) {
        burnout.zone  = BurnoutZone::Danger;
        burnout.meter = 0.7f + (1.0f - (nearest_dist - danger_min) / (risky_min - danger_min)) * 0.25f;
    } else {
        burnout.zone  = BurnoutZone::Dead;
        burnout.meter = 1.0f;
    }

    // Push zone change SFX
    if (burnout.zone != prev_zone) {
        auto& audio = reg.ctx().get<AudioQueue>();
        if (burnout.zone == BurnoutZone::Risky)  audio_push(audio, SFX::ZoneRisky);
        if (burnout.zone == BurnoutZone::Danger) audio_push(audio, SFX::ZoneDanger);
        if (burnout.zone == BurnoutZone::Dead)   audio_push(audio, SFX::ZoneDead);
    }
}
