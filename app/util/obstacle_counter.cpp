#include "obstacle_counter.h"
#include "../components/obstacle.h"
#include <entt/entt.hpp>

// ── Signal listeners ─────────────────────────────────────────────────────────
// These are wired once per registry lifetime by wire_obstacle_counter().
// Keeping them out of the header ensures ObstacleCounter remains plain data.

static void on_obstacle_tag_constructed(entt::registry& reg, entt::entity /*e*/) {
    if (auto* oc = reg.ctx().find<ObstacleCounter>()) {
        ++oc->count;
    }
}

static void on_obstacle_tag_destroyed(entt::registry& reg, entt::entity /*e*/) {
    if (auto* oc = reg.ctx().find<ObstacleCounter>()) {
        if (oc->count > 0) --oc->count;
    }
}

// Call once per registry lifetime to wire the signals.
// Safe to call multiple times — subsequent calls are no-ops (guarded by wired flag).
void wire_obstacle_counter(entt::registry& reg) {
    auto* oc = reg.ctx().find<ObstacleCounter>();
    if (!oc || oc->wired) return;
    reg.on_construct<ObstacleTag>().connect<&on_obstacle_tag_constructed>();
    reg.on_destroy<ObstacleTag>().connect<&on_obstacle_tag_destroyed>();
    oc->wired = true;
}
