#pragma once

#include <cstdint>

// Haptic feedback events — sourced from design-docs/game-flow.md Appendix B.
// All haptics are opt-out (enabled by default).
// Players with haptics OFF receive 0 ms vibration (no fallback) per spec.
enum class HapticEvent : uint8_t {
    ShapeShift  = 0,  // Light tap       — player changes shape
    LaneSwitch,       // Ultra-light tap — player changes lane
    JumpLand,         // Light tap       — player lands from a jump (not takeoff)
    DeathCrash,       // Double-pulse Heavy — player death
    NewHighScore,     // Triple-tap Medium  — new personal high score achieved
    RetryTap,         // Crisp tap       — end-screen Restart pressed
    UIButtonTap,      // Ultra-light tap — any UI menu button pressed
};
