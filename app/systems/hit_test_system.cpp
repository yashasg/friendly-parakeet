#include "all_systems.h"
#include "../components/input_events.h"
#include "../components/input.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include <raylib.h>

// Resolves Tap inputs in the EventQueue against active UI HitBox/HitCircle
// entities and emits ButtonPressEvents. Swipe and other input types are
// intentionally ignored — gesture routing is owned by gesture_routing_system
// (see #272). Active-phase filtering is structural via ActiveTag (see #249).
void hit_test_system(entt::registry& reg) {
    auto& eq   = reg.ctx().get<EventQueue>();
    auto& disp = reg.ctx().get<entt::dispatcher>();

    // Sync structural ActiveTag against current phase. Cheap when phase has
    // not changed since the last sync; the per-event loops below then iterate
    // only the active subset with no runtime predicate.
    ensure_active_tags_synced(reg);

    auto box_view    = reg.view<Position, HitBox,    ActiveTag>();
    auto circle_view = reg.view<Position, HitCircle, ActiveTag>();

    for (int i = 0; i < eq.input_count; ++i) {
        const auto& evt = eq.inputs[i];

        if (evt.type != InputType::Tap) continue;

        Vector2 point = {evt.x, evt.y};

        // Tap: hit-test against active HitBox entities
        for (auto [entity, pos, hb] : box_view.each()) {
            Rectangle bounds = {
                pos.x - hb.half_w,
                pos.y - hb.half_h,
                hb.half_w * 2.0f,
                hb.half_h * 2.0f
            };
            if (CheckCollisionPointRec(point, bounds)) {
                disp.enqueue<ButtonPressEvent>({entity});
            }
        }

        // Tap: hit-test against active HitCircle entities
        for (auto [entity, pos, hc] : circle_view.each()) {
            if (CheckCollisionPointCircle(point, {pos.x, pos.y}, hc.radius)) {
                disp.enqueue<ButtonPressEvent>({entity});
            }
        }
    }
}
