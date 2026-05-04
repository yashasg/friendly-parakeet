#include "all_systems.h"
#include "../input/raylib_gesture_input.h"
#include "../input/input_state.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include <raylib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

struct WebInputPolicy {
    bool prefers_touch = false;
};

} // namespace

void input_system_init(entt::registry& reg) {
    auto& policy = reg.ctx().emplace<WebInputPolicy>();
#if defined(PLATFORM_WEB) && defined(__EMSCRIPTEN__)
    policy.prefers_touch = (EM_ASM_INT({
        const ua = navigator.userAgent || "";
        const mobile = /Android|iPhone|iPad|iPod|webOS|BlackBerry|IEMobile|Opera Mini/i.test(ua);
        const touchCapable = (navigator.maxTouchPoints || 0) > 0;
        return mobile && touchCapable ? 1 : 0;
    }) != 0);
#else
    (void)policy;
#endif
}

void input_system(entt::registry& reg, float raw_dt) {
    auto& input = reg.ctx().get<InputState>();
    auto& st    = reg.ctx().get<ScreenTransform>();
    auto& disp  = reg.ctx().get<entt::dispatcher>();
    if (!input.gestures_configured) {
        SetGesturesEnabled(kGameplayGestureFlags);
        input.gestures_configured = true;
    }
    // Discard any InputEvents that were not consumed before this frame
    // (defensive guard — R7: phase transitions can leave events queued).
    disp.clear<InputEvent>();
    clear_input_events(input);

#if defined(PLATFORM_WEB) && defined(__EMSCRIPTEN__)
    const auto& web_policy = reg.ctx().get<WebInputPolicy>();
    const bool allow_mouse_input = !web_policy.prefers_touch;
    const bool allow_touch_input = web_policy.prefers_touch;
#else
    const bool allow_mouse_input = true;
    const bool allow_touch_input = true;
#endif

    // ── Mouse (desktop) — click-only semantics ─
    if (allow_mouse_input &&
        input.active_source != InputSource::Touch &&
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        input.click      = true;
        input.touching   = false;
        input.active_source = InputSource::None;
        const Vector2 pos = screen_to_virtual(GetMousePosition(), st);
        input.start_x = input.curr_x = input.end_x = pos.x;
        input.start_y = input.curr_y = input.end_y = pos.y;
        input.duration = 0.0f;
    }

    // ── Touch (mobile / web) — only when no mouse gesture is active ─
    if (allow_touch_input &&
        input.active_source != InputSource::Mouse &&
        GetTouchPointCount() > 0) {
        const Vector2 tp = screen_to_virtual(GetTouchPosition(0), st);
        if (!input.touching) {
            input.touch_down = true;
            input.touching   = true;
            input.active_source = InputSource::Touch;
            input.start_x = input.curr_x = tp.x;
            input.start_y = input.curr_y = tp.y;
            input.duration = 0.0f;
        } else if (input.active_source == InputSource::Touch) {
            input.curr_x = tp.x;
            input.curr_y = tp.y;
        }
    } else if (allow_touch_input &&
               input.active_source != InputSource::Mouse &&
               input.touching && input.active_source == InputSource::Touch) {
        input.touch_up  = true;
        input.touching  = false;
        input.active_source = InputSource::None;
        input.end_x = input.curr_x;
        input.end_y = input.curr_y;
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
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Circle,
                                       MenuActionKind::Confirm, 0});
    }
    if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_X)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Square,
                                       MenuActionKind::Confirm, 0});
    }
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_C)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Triangle,
                                       MenuActionKind::Confirm, 0});
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

    if (input.touching) {
        input.duration += raw_dt;
    }

    // Touch/mouse gesture → InputEvent enqueued to dispatcher; delivered to
    // gesture_routing_handle_input via
    // disp.update<InputEvent>() in game_loop_frame (#333).
    if (input.click) {
        disp.enqueue<InputEvent>(InputEvent{InputType::Tap, Direction::Up,
                                            input.end_x, input.end_y});
    } else if (input.touch_up) {
        const InputEvent event = input_event_from_raylib_gesture(
            read_detected_raylib_gesture(),
            input.start_y,
            input.end_x,
            input.end_y);
        disp.enqueue<InputEvent>(event);
    }
}
