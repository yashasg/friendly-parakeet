#include "all_systems.h"
#include "../components/haptics.h"
#include "../platform/haptics_backend.h"

void haptic_system(entt::registry& reg) {
    auto* hq = reg.ctx().find<HapticQueue>();
    if (!hq || hq->count == 0) return;

    for (int i = 0; i < hq->count; ++i) {
        platform::haptics::trigger(hq->queue[i]);
    }

    hq->count = 0;
}
