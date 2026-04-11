#pragma once

#include <cstdint>

// Emplaced ONLY on obstacles with RequiredShape (ShapeGate, ComboGate, SplitPath).
// Tracks which timing zone the obstacle's proximity ring is currently in,
// so the session logger can record zone transitions.
struct RingZoneTracker {
    int8_t last_zone   = -1;     // -1=none, 1=bad, 2=ok, 3=good, 4=perfect
    bool   past_center = false;  // true once obstacle passes perfect center distance
};
