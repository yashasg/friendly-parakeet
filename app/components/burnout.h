#pragma once

#include <cstdint>
#include <entt/entity/entity.hpp>

enum class BurnoutZone : uint8_t {
    None   = 0,
    Safe   = 1,
    Risky  = 2,
    Danger = 3,
    Dead   = 4
};

struct BurnoutState {
    float        meter           = 0.0f;
    BurnoutZone  zone            = BurnoutZone::None;
    float        threat_distance = 0.0f;
    entt::entity nearest_threat  = entt::null;
};

// Snapshotted at the moment the player commits the qualifying input that
// resolves a given obstacle.  Consumed by scoring_system instead of the
// live BurnoutState so the multiplier reflects when the player acted,
// not when the obstacle geometrically crosses the collision line.
struct BankedBurnout {
    float       multiplier;  // multiplier_for_zone(zone) at press time
    BurnoutZone zone;        // zone at press time (for popup / results)
};
