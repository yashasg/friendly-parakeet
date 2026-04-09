#pragma once

#include "player.h"
#include <cstdint>

struct RequiredShape {
    Shape shape = Shape::Circle;
};

struct BlockedLanes {
    uint8_t mask = 0;
};

struct RequiredLane {
    int8_t lane = 0;
};

struct RequiredVAction {
    VMode action = VMode::Jumping;
};
