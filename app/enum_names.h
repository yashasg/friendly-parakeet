#pragma once

#include "components/player.h"
#include "components/obstacle.h"
#include "components/rhythm.h"

// Canonical name functions for enums, used by logging and debug systems.
// Inlined to avoid adding a .cpp to the build for trivial string tables.

inline const char* shape_name(Shape s) {
    switch (s) {
        case Shape::Circle:   return "Circle";
        case Shape::Square:   return "Square";
        case Shape::Triangle: return "Triangle";
        case Shape::Hexagon:  return "Hexagon";
    }
    return "???";
}

inline const char* obstacle_kind_name(ObstacleKind k) {
    switch (k) {
        case ObstacleKind::ShapeGate: return "ShapeGate";
        case ObstacleKind::LaneBlock: return "LaneBlock";
        case ObstacleKind::LowBar:    return "LowBar";
        case ObstacleKind::HighBar:   return "HighBar";
        case ObstacleKind::ComboGate: return "ComboGate";
        case ObstacleKind::SplitPath: return "SplitPath";
        case ObstacleKind::LanePushLeft:  return "LanePushLeft";
        case ObstacleKind::LanePushRight: return "LanePushRight";
    }
    return "???";
}

inline const char* timing_tier_name(TimingTier t) {
    switch (t) {
        case TimingTier::Bad:     return "Bad";
        case TimingTier::Ok:      return "Ok";
        case TimingTier::Good:    return "Good";
        case TimingTier::Perfect: return "Perfect";
    }
    return "???";
}
