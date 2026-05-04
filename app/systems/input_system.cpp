#include "all_systems.h"
#include "../input/raylib_gesture_input.h"
#include "../input/input_state.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include "../platform/input/input_handler.h"
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
    auto& platform_input = platform::input::input_handler();
    if (!input.gestures_configured) {
        platform_input.configure_gameplay_gestures();
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
        platform_input.is_mouse_left_released()) {
        input.click      = true;
        input.touching   = false;
        input.active_source = InputSource::None;
        const auto mouse_position = platform_input.mouse_position();
        const Vector2 pos = screen_to_virtual({mouse_position.x, mouse_position.y}, st);
        input.start_x = input.curr_x = input.end_x = pos.x;
        input.start_y = input.curr_y = input.end_y = pos.y;
        input.duration = 0.0f;
    }

    // ── Touch (mobile / web) — only when no mouse gesture is active ─
    if (allow_touch_input &&
        input.active_source != InputSource::Mouse &&
        platform_input.touch_point_count() > 0) {
        const auto touch_position = platform_input.touch_position(0);
        const Vector2 tp = screen_to_virtual({touch_position.x, touch_position.y}, st);
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
    using platform::input::KeyCode;
    if (platform_input.is_key_pressed(KeyCode::W) || platform_input.is_key_pressed(KeyCode::Up)) {
        disp.enqueue<GoEvent>({Direction::Up});
    }
    if (platform_input.is_key_pressed(KeyCode::S) || platform_input.is_key_pressed(KeyCode::Down)) {
        disp.enqueue<GoEvent>({Direction::Down});
    }
    if (platform_input.is_key_pressed(KeyCode::A) || platform_input.is_key_pressed(KeyCode::Left)) {
        disp.enqueue<GoEvent>({Direction::Left});
    }
    if (platform_input.is_key_pressed(KeyCode::D) || platform_input.is_key_pressed(KeyCode::Right)) {
        disp.enqueue<GoEvent>({Direction::Right});
    }

    // Keyboard shape-button presses: encode semantic payload directly — no
    // entity lookup needed (#273).
    if (platform_input.is_key_pressed(KeyCode::One) || platform_input.is_key_pressed(KeyCode::Z)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Circle,
                                       MenuActionKind::Confirm, 0});
    }
    if (platform_input.is_key_pressed(KeyCode::Two) || platform_input.is_key_pressed(KeyCode::X)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Square,
                                       MenuActionKind::Confirm, 0});
    }
    if (platform_input.is_key_pressed(KeyCode::Three) || platform_input.is_key_pressed(KeyCode::C)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Triangle,
                                       MenuActionKind::Confirm, 0});
    }
    if (platform_input.is_key_pressed(KeyCode::Enter) || platform_input.is_key_pressed(KeyCode::Space)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                       MenuActionKind::Confirm, 0});
    }
#endif

    // ── Background / suspend (edge-triggered) ─────────────
    // Pause only on the frame focus is *lost*, not every frame while unfocused.
    {
        bool focused = platform_input.is_window_focused();
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
            platform_input.read_detected_gesture(),
            input.start_y,
            input.end_x,
            input.end_y);
        disp.enqueue<InputEvent>(event);
    }
}
