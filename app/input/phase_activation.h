#pragma once

#include "../components/input_events.h"

inline bool phase_active(const ActiveInPhase& aip, GamePhase phase) {
    return !!(aip.phase_mask & to_phase_bit(phase));
}
