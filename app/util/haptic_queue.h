#pragma once

#include "../components/haptics.h"

inline void haptic_clear(HapticQueue& q) { q.count = 0; }

// Push a haptic event, gated by the haptics_enabled flag.
// When haptics_enabled is false the call is a guaranteed 0 ms no-op per spec.
inline void haptic_push(HapticQueue& q, bool haptics_enabled, HapticEvent e) {
    if (!haptics_enabled) return;
    if (q.count < HapticQueue::MAX_QUEUED) q.queue[q.count++] = e;
}
