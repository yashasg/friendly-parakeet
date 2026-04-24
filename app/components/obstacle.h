#pragma once

#include <cstdint>

struct ObstacleTag {};

#define OBSTACLE_KIND_LIST(X) \
    X(ShapeGate)              \
    X(LaneBlock)              \
    X(LowBar)                 \
    X(HighBar)                \
    X(ComboGate)              \
    X(SplitPath)              \
    X(LanePushLeft)           \
    X(LanePushRight)

enum class ObstacleKind : uint8_t {
    #define OBSTACLE_KIND_ENUM(name) name,
    OBSTACLE_KIND_LIST(OBSTACLE_KIND_ENUM)
    #undef OBSTACLE_KIND_ENUM
};

inline const char* ToString(ObstacleKind k) {
    switch (k) {
        #define OBSTACLE_KIND_STR(name) case ObstacleKind::name: return #name;
        OBSTACLE_KIND_LIST(OBSTACLE_KIND_STR)
        #undef OBSTACLE_KIND_STR
    }
    return "???";
}

struct Obstacle {
    ObstacleKind kind       = ObstacleKind::ShapeGate;
    int16_t      base_points = 200;
};

// Existential tag: presence means the obstacle has been cleared and awaits scoring.
struct ScoredTag {};

// Existential tag: scored obstacle was failed/missed and should not award points.
struct MissTag {};

#undef OBSTACLE_KIND_LIST
