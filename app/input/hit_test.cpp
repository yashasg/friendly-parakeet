#include "input_routing.h"
#include "../components/input_events.h"
#include "../components/input.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include <raylib.h>

// Resolves Tap InputEvents against active UI HitBox/HitCircle entities and
// emits ButtonPressEvents with semantic value payloads (#273).  Swipe and
// other input types are intentionally ignored — gesture routing is owned by
// gesture_routing_handle_input (see #272).  Active-phase filtering is
// structural via ActiveTag (see #249).
//
// Registered as a disp.sink<InputEvent>() listener in wire_input_dispatcher()
// and fired via disp.update<InputEvent>() in game_loop_frame before the
// fixed-step accumulator loop (#333).
void hit_test_handle_input(entt::registry& reg, const InputEvent& evt) {
    if (evt.type != InputType::Tap) return;

    // Sync structural ActiveTag against current phase. Cheap (O(1)) when phase
    // has not changed since the last sync; subsequent calls within the same
    // disp.update<InputEvent>() batch are no-ops.
    ensure_active_tags_synced(reg);

    auto& disp = reg.ctx().get<entt::dispatcher>();
    Vector2 point = {evt.x, evt.y};

    // Tap: hit-test against active HitBox entities
    auto box_view = reg.view<UIPosition, HitBox, ActiveTag>();
    for (auto [entity, pos, hb] : box_view.each()) {
        Rectangle bounds = {
            pos.value.x - hb.half_w,
            pos.value.y - hb.half_h,
            hb.half_w * 2.0f,
            hb.half_h * 2.0f
        };
        if (!CheckCollisionPointRec(point, bounds)) continue;

        // Encode semantic payload at hit-test time — no live entity handle stored.
        if (reg.all_of<ShapeButtonTag, ShapeButtonData>(entity)) {
            auto shape = reg.get<ShapeButtonData>(entity).shape;
            disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, shape,
                                           MenuActionKind::Confirm, 0});
        } else if (reg.all_of<MenuButtonTag, MenuAction>(entity)) {
            auto& ma = reg.get<MenuAction>(entity);
            disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                           ma.kind, ma.index});
        }
    }

    // Tap: hit-test against active HitCircle entities
    auto circle_view = reg.view<UIPosition, HitCircle, ActiveTag>();
    for (auto [entity, pos, hc] : circle_view.each()) {
        if (!CheckCollisionPointCircle(point, pos.value, hc.radius)) continue;

        if (reg.all_of<ShapeButtonTag, ShapeButtonData>(entity)) {
            auto shape = reg.get<ShapeButtonData>(entity).shape;
            disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, shape,
                                           MenuActionKind::Confirm, 0});
        } else if (reg.all_of<MenuButtonTag, MenuAction>(entity)) {
            auto& ma = reg.get<MenuAction>(entity);
            disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                           ma.kind, ma.index});
        }
    }
}
