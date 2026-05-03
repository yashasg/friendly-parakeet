#include "all_systems.h"
#include "../components/player.h"
#include "../components/gameplay_intents.h"
#include "../constants.h"

// Consumes PendingLanePush events (emplaced by collision_system) and applies
// the lane transition to the player's Lane component. Runs in the same frame
// as collision_system, immediately after it, so the response is never deferred.
void lane_push_response_system(entt::registry& reg, float /*dt*/) {
    auto view = reg.view<PlayerTag, Lane, PendingLanePush>();
    for (auto [entity, lane, push] : view.each()) {
        if (lane.target < 0) {
            int8_t dest = static_cast<int8_t>(lane.current + push.delta);
            if (dest >= 0 && dest < constants::LANE_COUNT) {
                lane.target = dest;
                lane.lerp_t = 0.0f;
            }
        }
        reg.remove<PendingLanePush>(entity);
    }
}
