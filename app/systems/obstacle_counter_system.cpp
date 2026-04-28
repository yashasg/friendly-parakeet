#include "all_systems.h"
#include "obstacle_counter_system.h"
#include "../components/obstacle.h"
#include <entt/entt.hpp>

// ── Signal listeners ─────────────────────────────────────────────────────────
// These are wired once per registry lifetime by wire_obstacle_counter().
// Keeping them in the system translation unit ensures ObstacleCounter remains
// a plain data component with no logic in the header (#330).

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
