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
