#pragma once

#include <array>
#include <cstddef>
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

// Stringification for trace/log lines. Replaces the former
// `magic_enum::enum_name(HapticEvent)` call site in haptics_backend.cpp.
//
// Fabian Principle 1 / issue #1309: enum-as-lookup-key into a static table.
// Row order must match the `HapticEvent` declaration above; the
// `static_assert` pins the table size to the trailing enumerator
// (`UIButtonTap`).
constexpr std::string_view to_string(HapticEvent event) noexcept {
    constexpr std::array<std::string_view, 7> kNameByEvent{{
        /* ShapeShift   */ "ShapeShift",
        /* LaneSwitch   */ "LaneSwitch",
        /* JumpLand     */ "JumpLand",
        /* DeathCrash   */ "DeathCrash",
        /* NewHighScore */ "NewHighScore",
        /* RetryTap     */ "RetryTap",
        /* UIButtonTap  */ "UIButtonTap",
    }};
    static_assert(kNameByEvent.size() ==
                  static_cast<std::size_t>(HapticEvent::UIButtonTap) + 1,
                  "kNameByEvent must cover every HapticEvent enumerator");

    const auto idx = static_cast<std::size_t>(event);
    if (idx >= kNameByEvent.size()) return {};
    return kNameByEvent[idx];
}
