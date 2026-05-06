#include "obstacle_counter.h"
#include "../components/obstacle.h"
#include <entt/entt.hpp>

namespace {

// ── Signal listeners ─────────────────────────────────────────────────────────
// These are wired once per registry lifetime by wire_obstacle_counter().
// Keeping them out of the header ensures ObstacleCounter remains plain data.

void on_obstacle_tag_constructed(entt::registry& reg, entt::entity /*e*/) {
    if (auto* oc = reg.ctx().find<ObstacleCounter>()) {
        ++oc->count;
    }
}

void on_obstacle_tag_destroyed(entt::registry& reg, entt::entity /*e*/) {
    auto* oc = reg.ctx().find<ObstacleCounter>();
    if (!oc || oc->count <= 0) return;
    --oc->count;
}

}  // namespace

// Call once per registry lifetime to wire the signals.
// Safe to call multiple times — subsequent calls are no-ops (guarded by wired flag).
void wire_obstacle_counter(entt::registry& reg) {
    auto* counter = reg.ctx().find<ObstacleCounter>();
    if (!counter || counter->wired) return;
    reg.on_construct<ObstacleTag>().connect<&on_obstacle_tag_constructed>();
    reg.on_destroy<ObstacleTag>().connect<&on_obstacle_tag_destroyed>();
    counter->wired = true;
}
