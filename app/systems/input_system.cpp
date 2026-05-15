#include "all_systems.h"
#include "camera_system.h"
#include "input_system_private.h"
#include "web_input_policy.h"
#include "../util/keyboard_shape_mapping.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include <cmath>
#include <raylib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

constexpr unsigned int kGameplayGestureFlags =
    GESTURE_TAP
    | GESTURE_SWIPE_RIGHT
    | GESTURE_SWIPE_LEFT
    | GESTURE_SWIPE_UP
    | GESTURE_SWIPE_DOWN;

struct WebInputPolicy {
    // True iff the browser reports the device exposes a touch surface
    // (navigator.maxTouchPoints > 0). This is a stable, per-session
    // capability flag — NOT a "prefer touch over mouse" latch. Both
    // mouse and touch input remain enabled on web so that:
    //  • iPadOS 13+ Safari (which reports a desktop UA but
    //    maxTouchPoints>0) still receives touch/swipe gameplay events.
    //  • Touchscreen laptops/Chromebooks/Surface devices accept touch
    //    regardless of UA string.
    //  • A USB/Bluetooth mouse plugged into a touch-capable device is
    //    not silently ignored for the rest of the session.
    // Per-frame routing between mouse and touch is handled by the
    // active_source guard inside input_system.
    bool touch_capable = false;
};

bool classify_swipe(float dx, float dy, float duration, Direction& out) {
    const float distance_sq = dx * dx + dy * dy;
    if (distance_sq < constants::MIN_SWIPE_DIST * constants::MIN_SWIPE_DIST ||
        duration > constants::MAX_SWIPE_TIME) {
        return false;
    }

    if (std::fabs(dx) >= std::fabs(dy)) {
        out = (dx >= 0.0f) ? Direction::Right : Direction::Left;
    } else {
        out = (dy >= 0.0f) ? Direction::Down : Direction::Up;
    }
    return true;
}

bool raylib_gesture_swipe_direction(Direction& out) {
    const int gesture = GetGestureDetected();
    if (IsGestureDetected(GESTURE_SWIPE_RIGHT) || (gesture & GESTURE_SWIPE_RIGHT) != 0) {
        out = Direction::Right;
        return true;
    }
    if (IsGestureDetected(GESTURE_SWIPE_LEFT) || (gesture & GESTURE_SWIPE_LEFT) != 0) {
        out = Direction::Left;
        return true;
    }
    if (IsGestureDetected(GESTURE_SWIPE_UP) || (gesture & GESTURE_SWIPE_UP) != 0) {
        out = Direction::Up;
        return true;
    }
    if (IsGestureDetected(GESTURE_SWIPE_DOWN) || (gesture & GESTURE_SWIPE_DOWN) != 0) {
        out = Direction::Down;
        return true;
    }

    const Vector2 drag = GetGestureDragVector();
    return classify_swipe(drag.x, drag.y, GetGestureHoldDuration(), out);
}

int find_touch_slot(InputState& input, int touch_id) {
    for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
        if (input.touch_slots[i].active && input.touch_slots[i].id == touch_id) {
            return i;
        }
    }
    return -1;
}

int find_free_touch_slot(InputState& input) {
    for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
        if (!input.touch_slots[i].active) {
            return i;
        }
    }
    return -1;
}

bool touch_id_is_current(int touch_id, int touch_point_count) {
    for (int touch_index = 0; touch_index < touch_point_count; ++touch_index) {
        if (GetTouchPointId(touch_index) == touch_id) {
            return true;
        }
    }
    return false;
}

bool has_active_touch_in_zone(const InputState& input, bool button_zone) {
    for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
        const auto& slot = input.touch_slots[i];
        if (slot.active && slot.started_in_button_zone == button_zone) {
            return true;
        }
    }
    return false;
}

void release_touch_slot(TouchSlot& slot,
                        InputState& input,
                        Direction& latest_swipe_dir,
                        bool& has_latest_swipe,
                        bool& had_swipe_zone_release) {
    input.touch_up = true;
    input.end_x = slot.curr_x;
    input.end_y = slot.curr_y;
    if (slot.started_in_button_zone) {
        input.button_touch_up = true;
        input.button_end_x = slot.curr_x;
        input.button_end_y = slot.curr_y;
    } else {
        had_swipe_zone_release = true;
        if (classify_swipe(slot.curr_x - slot.start_x,
                           slot.curr_y - slot.start_y,
                           slot.duration,
                           latest_swipe_dir)) {
            has_latest_swipe = true;
        }
    }
    slot = TouchSlot{};
}

} // namespace

void input_system_init(entt::registry& reg) {
    auto& policy = reg.ctx().emplace<WebInputPolicy>();
#if defined(PLATFORM_WEB) && defined(__EMSCRIPTEN__)
    policy.touch_capable = (EM_ASM_INT({
        return ((navigator.maxTouchPoints || 0) > 0) ? 1 : 0;
    }) != 0);
#else
    (void)policy;
#endif
    reg.ctx().erase<InputSystemPrivate>();
    reg.ctx().emplace<InputSystemPrivate>();
}

bool web_input_touch_capable(const entt::registry& reg) {
    if (const auto* policy = reg.ctx().find<WebInputPolicy>()) {
        return policy->touch_capable;
    }
    return false;
}

void input_system_clear_pointer_state(entt::registry& reg) {
    if (auto* input = reg.ctx().find<InputState>()) {
        input->touch_down = false;
        input->touch_up = false;
        input->click = false;
        input->button_touch_up = false;
        input->touching = false;
        input->active_source = InputSource::None;
        input->duration = 0.0f;
        for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
            input->touch_slots[i] = TouchSlot{};
        }
    }
    if (auto* priv = reg.ctx().find<InputSystemPrivate>()) {
        priv->suppress_mouse_release = false;
    }
}

void input_system(entt::registry& reg, float raw_dt) {
    auto& input = reg.ctx().get<InputState>();
    auto& priv  = reg.ctx().get<InputSystemPrivate>();
    auto& st    = reg.ctx().get<ScreenTransform>();
    auto& disp  = reg.ctx().get<entt::dispatcher>();
    if (!priv.gestures_configured) {
        SetGesturesEnabled(kGameplayGestureFlags);
        priv.gestures_configured = true;
    }
    input.touch_down = false;
    input.touch_up   = false;
    input.click      = false;
    input.button_touch_up = false;

#if defined(PLATFORM_WEB) && defined(__EMSCRIPTEN__)
    const auto& web_policy = reg.ctx().get<WebInputPolicy>();
    // Mouse is always allowed on web. Touch is allowed whenever the
    // browser reports a touch surface. The active_source guard below
    // prevents a single gesture from being routed through both paths.
    const bool allow_mouse_input = true;
    const bool allow_touch_input = web_policy.touch_capable;
#else
    const bool allow_mouse_input = true;
    const bool allow_touch_input = true;
#endif
    const int touch_point_count = GetTouchPointCount();
    const bool mouse_released = allow_mouse_input && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    if (allow_mouse_input &&
        priv.suppress_mouse_release &&
        touch_point_count == 0 &&
        !mouse_released) {
        priv.suppress_mouse_release = false;
    }

    // ── Mouse (desktop) — click-only semantics ─
    if (allow_mouse_input &&
        input.active_source != InputSource::Touch &&
        !priv.suppress_mouse_release &&
        touch_point_count == 0 &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        input.active_source = InputSource::Mouse;
        priv.suppress_mouse_release = false;
    }
    if (allow_mouse_input &&
        input.active_source != InputSource::Touch &&
        mouse_released) {
        const bool suppress_this_release = priv.suppress_mouse_release;
        input.click      = !suppress_this_release;
        input.touching   = false;
        input.active_source = InputSource::None;
        priv.suppress_mouse_release = false;
        const Vector2 mouse_pos = GetMousePosition();
        const Vector2 pos = screen_to_virtual({mouse_pos.x, mouse_pos.y}, st);
        input.start_x = input.curr_x = input.end_x = pos.x;
        input.start_y = input.curr_y = input.end_y = pos.y;
        input.duration = 0.0f;
    }

    // ── Touch (mobile / web) — up to one swipe-zone and one button-zone contact ─
    if (allow_touch_input &&
        input.active_source != InputSource::Mouse &&
        touch_point_count > 0) {
        const float zone_y = constants::SCREEN_H_F * constants::SWIPE_ZONE_SPLIT;
        Direction latest_swipe_dir = Direction::Up;
        bool has_latest_swipe = false;
        bool had_swipe_zone_release = false;

        for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
            auto& slot = input.touch_slots[i];
            if (slot.active && !touch_id_is_current(slot.id, touch_point_count)) {
                release_touch_slot(slot,
                                   input,
                                   latest_swipe_dir,
                                   has_latest_swipe,
                                   had_swipe_zone_release);
            }
        }

        const int points_to_scan = touch_point_count;
        for (int touch_index = 0; touch_index < points_to_scan; ++touch_index) {
            const int touch_id = GetTouchPointId(touch_index);
            const Vector2 touch_pos = GetTouchPosition(touch_index);
            const Vector2 tp = screen_to_virtual({touch_pos.x, touch_pos.y}, st);
            int slot_index = find_touch_slot(input, touch_id);
            if (slot_index < 0) {
                const bool button_zone = tp.y >= zone_y;
                if (has_active_touch_in_zone(input, button_zone)) {
                    continue;
                }
                slot_index = find_free_touch_slot(input);
            }
            if (slot_index < 0) {
                continue;
            }

            auto& slot = input.touch_slots[slot_index];
            if (!slot.active) {
                slot.id = touch_id;
                slot.active = true;
                slot.started_in_button_zone = tp.y >= zone_y;
                slot.start_x = slot.curr_x = tp.x;
                slot.start_y = slot.curr_y = tp.y;
                slot.duration = 0.0f;
                input.touch_down = true;
            } else {
                slot.curr_x = tp.x;
                slot.curr_y = tp.y;
            }
            input.start_x = slot.start_x;
            input.start_y = slot.start_y;
            input.curr_x = tp.x;
            input.curr_y = tp.y;
        }

        if (!has_latest_swipe &&
            had_swipe_zone_release &&
            raylib_gesture_swipe_direction(latest_swipe_dir)) {
            has_latest_swipe = true;
        }
        if (has_latest_swipe) {
            disp.enqueue<GoEvent>(GoEvent{latest_swipe_dir});
        }

        input.touching = false;
        for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
            if (input.touch_slots[i].active) {
                input.touching = true;
                input.touch_slots[i].duration += raw_dt;
            }
        }
        if (input.touching) {
            input.active_source = InputSource::Touch;
        }
        input.duration = 0.0f;
        for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
            if (input.touch_slots[i].active && input.touch_slots[i].duration > input.duration) {
                input.duration = input.touch_slots[i].duration;
            }
        }
    } else if (allow_touch_input &&
               input.active_source != InputSource::Mouse &&
               input.touching && input.active_source == InputSource::Touch) {
        Direction latest_swipe_dir = Direction::Up;
        bool has_latest_swipe = false;
        bool had_swipe_zone_release = false;
        input.touch_up  = true;
        input.touching  = false;
        priv.suppress_mouse_release = true;
        input.active_source = InputSource::None;
        for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
            auto& slot = input.touch_slots[i];
            if (!slot.active) {
                continue;
            }
            release_touch_slot(slot,
                               input,
                               latest_swipe_dir,
                               has_latest_swipe,
                               had_swipe_zone_release);
        }
        if (!has_latest_swipe &&
            had_swipe_zone_release &&
            raylib_gesture_swipe_direction(latest_swipe_dir)) {
            has_latest_swipe = true;
        }
        if (has_latest_swipe) {
            disp.enqueue<GoEvent>(GoEvent{latest_swipe_dir});
        }
        input.duration = 0.0f;
    }

#ifdef PLATFORM_HAS_KEYBOARD
    // ── Keyboard — IsKeyPressed fires once per press (no repeat) ──
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    { disp.enqueue<GoEvent>({Direction::Up}); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  { disp.enqueue<GoEvent>({Direction::Down}); }
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  { disp.enqueue<GoEvent>({Direction::Left}); }
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { disp.enqueue<GoEvent>({Direction::Right}); }

    // Keyboard shape-button presses: encode semantic payload directly — no
    // entity lookup needed (#273).
    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_Z)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape,
                                        shape_for_keyboard_slot(KeyboardShapeSlot::Left),
                                        MenuActionKind::Confirm,
                                        0});
    }
    if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_X)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape,
                                        shape_for_keyboard_slot(KeyboardShapeSlot::Center),
                                        MenuActionKind::Confirm,
                                        0});
    }
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_C)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape,
                                        shape_for_keyboard_slot(KeyboardShapeSlot::Right),
                                        MenuActionKind::Confirm,
                                        0});
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                       MenuActionKind::Confirm, 0});
    }
#endif

    // ── Background / suspend (edge-triggered) ─────────────
    // Pause only on the frame focus is *lost*, not every frame while unfocused.
    {
        bool focused = IsWindowFocused();
        if (priv.was_focused && !focused &&
            reg.ctx().get<GameState>().phase == GamePhase::Playing) {
            auto& gs = reg.ctx().get<GameState>();
            gs.transition_pending = true;
            gs.next_phase = GamePhase::Paused;
        }
        priv.was_focused = focused;
    }

    if (input.active_source != InputSource::Mouse &&
        !input.touching &&
        !input.touch_up) {
        Direction gesture_dir = Direction::Up;
        const bool has_gesture_swipe = raylib_gesture_swipe_direction(gesture_dir);

        // Gesture fired after the touch already ended: use the captured swipe
        // start (set at touch-down) instead of GetTouchPosition(0), which would
        // return stale data or fall back to the mouse cursor on raylib 5.x.
        const float zone_threshold = constants::SCREEN_H_F * constants::SWIPE_ZONE_SPLIT;
        const float gesture_origin_y = input.click ? input.end_y : input.start_y;
        const bool gesture_started_in_swipe_zone = gesture_origin_y < zone_threshold;

        if (has_gesture_swipe && gesture_started_in_swipe_zone) {
            input.click = false;
            disp.enqueue<GoEvent>(GoEvent{gesture_dir});
        }
    }
}
