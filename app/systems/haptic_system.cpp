#include "all_systems.h"
#include "../components/haptics.h"
#include "../platform/haptics_backend.h"
#include "../util/haptic_queue.h"

void haptic_system(entt::registry& reg) {
    auto* hq = reg.ctx().find<HapticQueue>();
    if (!hq || hq->count == 0) return;

    for (int i = 0; i < hq->count; ++i) {
        platform::haptics::trigger(hq->queue[i]);
    }

    haptic_clear(*hq);
}
