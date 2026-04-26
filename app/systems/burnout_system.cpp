#include "all_systems.h"
#include "burnout_helpers.h"
#include "../components/game_state.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/difficulty.h"
#include "../components/audio.h"
#include "../components/haptics.h"
#include "../components/settings.h"
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

    BurnoutZone prev_zone = burnout.zone;

    auto sample   = compute_burnout_for_distance(nearest_dist, config.burnout_window_scale);
    burnout.zone  = sample.zone;
    burnout.meter = sample.meter;

    // Push zone change SFX and haptics
    if (burnout.zone != prev_zone) {
        auto& audio = reg.ctx().get<AudioQueue>();
        if (burnout.zone == BurnoutZone::Risky)  audio_push(audio, SFX::ZoneRisky);
        if (burnout.zone == BurnoutZone::Danger) audio_push(audio, SFX::ZoneDanger);
        if (burnout.zone == BurnoutZone::Dead)   audio_push(audio, SFX::ZoneDead);

        auto* hq = reg.ctx().find<HapticQueue>();
        auto* st = reg.ctx().find<SettingsState>();
        if (hq) {
            bool haptics_on = !st || st->haptics_enabled;
            if (burnout.zone == BurnoutZone::Risky)  haptic_push(*hq, haptics_on, HapticEvent::Burnout1_5x);
            if (burnout.zone == BurnoutZone::Danger) haptic_push(*hq, haptics_on, HapticEvent::Burnout3_0x);
            if (burnout.zone == BurnoutZone::Dead)   haptic_push(*hq, haptics_on, HapticEvent::Burnout5_0x);
        }
    }
}
