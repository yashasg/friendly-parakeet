#pragma once

// Shape proportions — maps Shape enum to scale factors.
// radius_scale: model scale multiplier (applied in model-to-world transform)
// height_ratio: height relative to radius (used by GenMesh at build time)
struct ShapeProps {
    float  radius_scale;
    float  height_ratio;
};

// Registry singleton config for mesh generation and transform scaling.
// Indexed via shape_index() from util/shape_lane_mapping.h.
struct ShapeMeshConfig {
    ShapeProps props[4] = {
        {0.5f, 0.6f},   // Circle:   cylinder
        {0.5f, 0.6f},   // Square:   cube
        {0.5f, 0.6f},   // Triangle: cone
        {0.6f, 1.17f},  // Hexagon:  cylinder (6 slices)
    };
};
