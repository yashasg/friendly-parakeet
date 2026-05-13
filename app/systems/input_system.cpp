#include "all_systems.h"
#include "web_input_policy.h"
#include "../input/keyboard_shape_mapping.h"
#include "../components/input.h"
#include "../components/input_events.h"
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
}

bool web_input_touch_capable(const entt::registry& reg) {
    if (const auto* policy = reg.ctx().find<WebInputPolicy>()) {
        return policy->touch_capable;
    }
    return false;
}

void input_system(entt::registry& reg, float raw_dt) {
    auto& input = reg.ctx().get<InputState>();
    auto& st    = reg.ctx().get<ScreenTransform>();
    auto& disp  = reg.ctx().get<entt::dispatcher>();
    if (!input.gestures_configured) {
        SetGesturesEnabled(kGameplayGestureFlags);
        input.gestures_configured = true;
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
        input.suppress_mouse_release &&
        touch_point_count == 0 &&
        !mouse_released) {
        input.suppress_mouse_release = false;
    }

    // ── Mouse (desktop) — click-only semantics ─
    if (allow_mouse_input &&
        input.active_source != InputSource::Touch &&
        !input.suppress_mouse_release &&
        touch_point_count == 0 &&
        IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        input.active_source = InputSource::Mouse;
        input.suppress_mouse_release = false;
    }
    if (allow_mouse_input &&
        input.active_source != InputSource::Touch &&
        mouse_released) {
        const bool suppress_this_release = input.suppress_mouse_release;
        input.click      = !suppress_this_release;
        input.touching   = false;
        input.active_source = InputSource::None;
        input.suppress_mouse_release = false;
        const Vector2 mouse_pos = GetMousePosition();
        const glm::vec2 pos = screen_to_virtual({mouse_pos.x, mouse_pos.y}, st);
        input.start_x = input.curr_x = input.end_x = pos.x;
        input.start_y = input.curr_y = input.end_y = pos.y;
        input.duration = 0.0f;
    }

    // ── Touch (mobile / web) — up to one swipe-zone and one button-zone contact ─
    if (allow_touch_input &&
        input.active_source != InputSource::Mouse &&
        touch_point_count > 0) {
        const float zone_y = constants::SCREEN_H_F * constants::SWIPE_ZONE_SPLIT;
        bool seen[InputState::MaxTrackedTouches] = {};
        const int points_to_scan = touch_point_count;
        for (int touch_index = 0; touch_index < points_to_scan; ++touch_index) {
            const int touch_id = GetTouchPointId(touch_index);
            int slot_index = find_touch_slot(input, touch_id);
            if (slot_index < 0) {
                slot_index = find_free_touch_slot(input);
            }
            if (slot_index < 0) {
                continue;
            }

            const Vector2 touch_pos = GetTouchPosition(touch_index);
            const glm::vec2 tp = screen_to_virtual({touch_pos.x, touch_pos.y}, st);
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
            seen[slot_index] = true;
            input.start_x = slot.start_x;
            input.start_y = slot.start_y;
            input.curr_x = tp.x;
            input.curr_y = tp.y;
        }

        Direction latest_swipe_dir = Direction::Up;
        bool has_latest_swipe = false;
        for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
            if (input.touch_slots[i].active && !seen[i]) {
                auto& slot = input.touch_slots[i];
                input.touch_up = true;
                input.end_x = slot.curr_x;
                input.end_y = slot.curr_y;
                if (slot.started_in_button_zone) {
                    input.button_touch_up = true;
                    input.button_end_x = slot.curr_x;
                    input.button_end_y = slot.curr_y;
                } else {
                    if (classify_swipe(slot.curr_x - slot.start_x,
                                       slot.curr_y - slot.start_y,
                                       slot.duration,
                                       latest_swipe_dir)) {
                        has_latest_swipe = true;
                    }
                }
                slot = TouchSlot{};
            }
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
        input.touch_up  = true;
        input.touching  = false;
        input.suppress_mouse_release = true;
        input.active_source = InputSource::None;
        for (int i = 0; i < InputState::MaxTrackedTouches; ++i) {
            auto& slot = input.touch_slots[i];
            if (!slot.active) {
                continue;
            }
            input.end_x = slot.curr_x;
            input.end_y = slot.curr_y;
            if (slot.started_in_button_zone) {
                input.button_touch_up = true;
                input.button_end_x = slot.curr_x;
                input.button_end_y = slot.curr_y;
            } else if (classify_swipe(slot.curr_x - slot.start_x,
                                      slot.curr_y - slot.start_y,
                                      slot.duration,
                                      latest_swipe_dir)) {
                has_latest_swipe = true;
            }
            slot = TouchSlot{};
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
        if (input.was_focused && !focused &&
            reg.ctx().get<GameState>().phase == GamePhase::Playing) {
            auto& gs = reg.ctx().get<GameState>();
            gs.transition_pending = true;
            gs.next_phase = GamePhase::Paused;
        }
        input.was_focused = focused;
    }

}
