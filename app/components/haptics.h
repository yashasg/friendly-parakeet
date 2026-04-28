#pragma once

#include <cstdint>

// Haptic feedback events — sourced from design-docs/game-flow.md Appendix B.
// All haptics are opt-out (enabled by default).
// Players with haptics OFF receive 0 ms vibration (no fallback) per spec.
//
// Burnout zone → multiplier mapping (see constants.h):
//   Risky  = 1.5×  →  Burnout1_5x
//   Danger = 3.0×  →  Burnout3_0x
//   Dead   = 5.0×  →  Burnout5_0x
// Burnout2_0x and Burnout4_0x are defined per spec but have no corresponding
// zone boundary in the current burnout model; they are reserved for future use.
enum class HapticEvent : uint8_t {
    ShapeShift  = 0,  // Light tap       — player changes shape
    LaneSwitch,       // Ultra-light tap — player changes lane
    JumpLand,         // Light tap       — player lands from a jump (not takeoff)
    Burnout1_5x,      // Light tap       — entering Risky zone  (1.5× multiplier)
    Burnout2_0x,      // Medium tap      — reserved (no zone boundary currently)
    Burnout3_0x,      // Medium tap      — entering Danger zone (3.0× multiplier)
    Burnout4_0x,      // Heavy impact    — reserved (no zone boundary currently)
    Burnout5_0x,      // Triple-pulse Heavy — entering Dead zone (5.0× multiplier)
    NearMiss,         // Heavy pulse     — scored in Dead zone (survived danger)
    DeathCrash,       // Double-pulse Heavy — player death
    NewHighScore,     // Triple-tap Medium  — new personal high score achieved
    RetryTap,         // Crisp tap       — end-screen Restart pressed
    UIButtonTap,      // Ultra-light tap — any UI menu button pressed
};

struct HapticQueue {
    static constexpr int MAX_QUEUED = 8;
    HapticEvent queue[MAX_QUEUED] = {};
    int count = 0;
};
