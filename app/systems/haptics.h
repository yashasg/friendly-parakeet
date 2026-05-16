#pragma once

#include <cstdint>
#include <string_view>

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

// Stringification for trace/log lines. Pure label lookup (no control-flow
// dispatch on the enum's value beyond the symbolic name), so the switch is
// doctrinally fine per Fabian's Existential Processing chapter (see issue
// #1204 — "Bonus: drop the magic_enum dependency"). Replaces the former
// `magic_enum::enum_name(HapticEvent)` call site in haptics_backend.cpp.
constexpr std::string_view to_string(HapticEvent event) noexcept {
    switch (event) {
        case HapticEvent::ShapeShift:   return "ShapeShift";
        case HapticEvent::LaneSwitch:   return "LaneSwitch";
        case HapticEvent::JumpLand:     return "JumpLand";
        case HapticEvent::DeathCrash:   return "DeathCrash";
        case HapticEvent::NewHighScore: return "NewHighScore";
        case HapticEvent::RetryTap:     return "RetryTap";
        case HapticEvent::UIButtonTap:  return "UIButtonTap";
    }
    return {};
}
