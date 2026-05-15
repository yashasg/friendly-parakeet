#pragma once

#include "../constants.h"
#include "tags/tags.h"

// Energy-bar data: the gameplay-resource singleton (EnergyState) and the
// per-frame visual contract consumed by the gameplay HUD renderer
// (EnergyBarLayout / EnergyBarVisual).

// ── Energy State (singleton) ────────────────────────
// Survival energy resource. Mutated by gameplay systems (collision, energy);
// rendered via the smoothed EnergyBarVisual layer below.
struct EnergyState {
    float energy      = constants::ENERGY_START;   // [0.0, 1.0] — current energy
    float display     = constants::ENERGY_START;   // smoothed for rendering (lerps toward energy)
    float flash_timer = 0.0f;   // > 0 when bar should flash (drain event)
};

struct EnergyBarLayout {
    float x = 16.0f;
    float width = 14.0f;
    float bottom = 965.0f;
    float height = 180.0f;
    int segment_count = 32;
    float segment_gap = 1.0f;
};

struct EnergyBarVisual {
    float fill = 1.0f;
    float visible_level = 1.0f;
    float flash_ratio = 0.0f;
    float flash_overlay = 0.0f;
    float critical_intensity = 0.0f;
    int overflow_segments = 0;
};

[[nodiscard]] inline int effective_energy_bar_segment_count(const EnergyBarLayout& layout) {
    return layout.segment_count < 1 ? 1 : layout.segment_count;
}
