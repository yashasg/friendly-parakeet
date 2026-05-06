#include "all_systems.h"
#include "../components/haptics.h"
#include "../components/registry_context.h"
#include "../systems/haptics_runtime.h"

void haptic_system(entt::registry& reg) {
    auto* hq = registry_ctx_find<HapticQueue>(reg);
    if (!hq || hq->count == 0) return;

    for (int i = 0; i < hq->count; ++i) {
        haptics_runtime::trigger(hq->queue[i]);
    }

    hq->clear();
}
