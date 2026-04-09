#pragma once

#include <cstdint>

struct ObstacleTag {};

enum class ObstacleKind : uint8_t {
    ShapeGate = 0,
    LaneBlock = 1,
    LowBar    = 2,
    HighBar   = 3,
    ComboGate = 4,
    SplitPath = 5
};

struct Obstacle {
    ObstacleKind kind       = ObstacleKind::ShapeGate;
    int16_t      base_points = 200;
    bool         scored      = false;
};
