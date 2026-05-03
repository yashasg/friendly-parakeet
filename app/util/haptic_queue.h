#pragma once

#include "../components/haptics.h"
#include "settings.h"
#include <entt/entt.hpp>

inline void haptic_clear(HapticQueue& q) { q.count = 0; }

// Push a haptic event, gated by the haptics_enabled flag.
// When haptics_enabled is false the call is a guaranteed 0 ms no-op per spec.
inline void haptic_push(HapticQueue& q, bool haptics_enabled, HapticEvent e) {
    if (!haptics_enabled) return;
    if (q.count < HapticQueue::MAX_QUEUED) q.queue[q.count++] = e;
}

inline bool haptics_enabled(const entt::registry& reg) {
    auto* st = reg.ctx().find<SettingsState>();
    return !st || st->haptics_enabled;
}

inline void haptic_feedback(entt::registry& reg, HapticEvent e) {
    auto* hq = reg.ctx().find<HapticQueue>();
    if (!hq) return;
    haptic_push(*hq, haptics_enabled(reg), e);
}
