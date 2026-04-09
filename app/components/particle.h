#pragma once

#include "rendering.h"

struct ParticleTag {};

struct ParticleData {
    float size       = 4.0f;
    float start_size = 4.0f;
};

struct ParticleEmitter {
    bool  active      = false;
    float emit_rate   = 0.0f;
    float accumulator = 0.0f;
    Color color       = { 255, 255, 255, 255 };
};
