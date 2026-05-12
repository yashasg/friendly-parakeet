#pragma once

// Entity-backed HUD state for the survival energy meter.
// EnergyState remains the gameplay resource; these components hold only the
// per-frame visual contract consumed by the gameplay HUD renderer.

struct EnergyBarTag {};

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
