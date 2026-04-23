#pragma once
#include <cstdint>

namespace shape_verts {

struct V2 { float x, y; };

// Circle: 24 perimeter points, unit radius, starting at 0° going CCW
constexpr int CIRCLE_SEGMENTS = 24;
constexpr V2 CIRCLE[CIRCLE_SEGMENTS] = {
    { 1.000000f,  0.000000f}, { 0.965926f,  0.258819f},
    { 0.866025f,  0.500000f}, { 0.707107f,  0.707107f},
    { 0.500000f,  0.866025f}, { 0.258819f,  0.965926f},
    { 0.000000f,  1.000000f}, {-0.258819f,  0.965926f},
    {-0.500000f,  0.866025f}, {-0.707107f,  0.707107f},
    {-0.866025f,  0.500000f}, {-0.965926f,  0.258819f},
    {-1.000000f,  0.000000f}, {-0.965926f, -0.258819f},
    {-0.866025f, -0.500000f}, {-0.707107f, -0.707107f},
    {-0.500000f, -0.866025f}, {-0.258819f, -0.965926f},
    { 0.000000f, -1.000000f}, { 0.258819f, -0.965926f},
    { 0.500000f, -0.866025f}, { 0.707107f, -0.707107f},
    { 0.866025f, -0.500000f}, { 0.965926f, -0.258819f},
};

// Hexagon: 6 perimeter points, pointy-top (-90° offset), unit radius
constexpr int HEX_SEGMENTS = 6;
constexpr V2 HEXAGON[HEX_SEGMENTS] = {
    { 0.000000f, -1.000000f},
    { 0.866025f, -0.500000f},
    { 0.866025f,  0.500000f},
    { 0.000000f,  1.000000f},
    {-0.866025f,  0.500000f},
    {-0.866025f, -0.500000f},
};

// Square: 4 corners, unit half-extent
constexpr V2 SQUARE[4] = {
    {-1.0f, -1.0f},  // top-left
    { 1.0f, -1.0f},  // top-right
    { 1.0f,  1.0f},  // bot-right
    {-1.0f,  1.0f},  // bot-left
};

// Triangle: 3 vertices, unit half-extent
constexpr V2 TRIANGLE[3] = {
    { 0.0f, -1.0f},  // apex
    {-1.0f,  1.0f},  // base-left
    { 1.0f,  1.0f},  // base-right
};

} // namespace shape_verts
