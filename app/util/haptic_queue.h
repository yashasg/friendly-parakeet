#pragma once

#include "../components/haptics.h"
#include "settings.h"
#include <entt/entt.hpp>

inline void haptic_feedback(entt::registry& reg, HapticEvent e) {
    auto* hq = reg.ctx().find<HapticQueue>();
    if (!hq) return;
    if (const auto* st = reg.ctx().find<SettingsState>(); st && !st->haptics_enabled) return;
    hq->push(e);
}
