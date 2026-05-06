#pragma once

#include <cstdint>

enum class WindowPhase : std::uint8_t {
    Idle = 0,
    MorphIn,
    Active,
    MorphOut,
};
