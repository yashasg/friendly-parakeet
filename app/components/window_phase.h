#pragma once

#include <cstdint>

// Shared header: WindowPhase enum only.
// Included by player.h (ShapeWindow component) and rhythm.h (full rhythm includes).
// Keeping this separate breaks the header cycle between the two component headers.
enum class WindowPhase : uint8_t {
    Idle     = 0,
    MorphIn  = 1,
    Active   = 2,
    MorphOut = 3
};
