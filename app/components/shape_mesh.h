#pragma once
#include "player.h"

// Shape proportions — maps Shape enum to scale factors.
// radius_scale: model scale multiplier (applied in model-to-world transform)
// height_ratio: height relative to radius (used by GenMesh at build time)
struct ShapeProps {
    float  radius_scale;
    float  height_ratio;
};

// Shape properties table indexed by Shape enum (Circle, Square, Triangle, Hexagon).
extern const ShapeProps SHAPE_PROPS[4];
