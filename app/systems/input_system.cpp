#include "gesture_input.h"
#include "input_latency_probe.h"
#include "../components/registry_context.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/transform.h"
#include "../components/game_state.h"
#include "../constants.h"
#include "../systems/platform_state.h"
#include <SDL.h>
#include <entt/entt.hpp>
#include <glm/vec2.hpp>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

struct WebInputPolicy {
    bool prefers_touch = false;
};

glm::vec2 to_virtual_point(float x, float y, const ScreenTransform& st) {
    return screen_to_virtual(glm::vec2{x, y}, st);
}

void begin_pointer_contact(InputState& input, const glm::vec2& point, const InputSource source) {
    input.touch_down = true;
    input.touching = true;
    input.active_source = source;
    input.start_x = input.curr_x = point.x;
    input.start_y = input.curr_y = point.y;
    input.duration = 0.0f;
}

void note_pointer_click(InputState& input, const glm::vec2& point) {
    input.click = true;
    input.touching = false;
    input.active_source = InputSource::None;
    input.start_x = input.curr_x = input.end_x = point.x;
    input.start_y = input.curr_y = input.end_y = point.y;
    input.duration = 0.0f;
}

void enqueue_go_if_pressed(entt::dispatcher& disp,
                           SDL_Scancode primary,
                           SDL_Scancode fallback,
                           Direction dir) {
    if (platform_state::input_key_pressed(primary) ||
        platform_state::input_key_pressed(fallback)) {
        disp.enqueue<GoEvent>({dir});
    }
}

void enqueue_shape_press_if_pressed(entt::dispatcher& disp,
                                    SDL_Scancode primary,
                                    SDL_Scancode fallback,
                                    Shape shape) {
    if (platform_state::input_key_pressed(primary) ||
        platform_state::input_key_pressed(fallback)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, shape,
                                        MenuActionKind::Confirm, 0});
    }
}

} // namespace

void input_system_init(entt::registry& reg) {
    registry_ctx_get_or_emplace<InputLatencyProbe>(reg);
    auto& policy = registry_ctx_get_or_emplace<WebInputPolicy>(reg);
    policy = WebInputPolicy{};
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
    input_latency_begin_frame(reg);
    auto& input = registry_ctx_get<InputState>(reg);
    auto& st    = registry_ctx_get<ScreenTransform>(reg);
    auto& disp  = registry_ctx_get<entt::dispatcher>(reg);
    platform_state::poll_events();
    if (!input.gestures_configured) {
        platform_state::input_configure_gameplay_gestures();
        input.gestures_configured = true;
    }
    // Discard any InputEvents that were not consumed before this frame
    // (defensive guard — R7: phase transitions can leave events queued).
    disp.clear<InputEvent>();
    clear_transient_input_flags(input);

#if defined(PLATFORM_WEB) && defined(__EMSCRIPTEN__)
    const auto& web_policy = registry_ctx_get<WebInputPolicy>(reg);
    const bool allow_mouse_input = !web_policy.prefers_touch;
    const bool allow_touch_input = web_policy.prefers_touch;
#else
    const bool allow_mouse_input = true;
    const bool allow_touch_input = true;
#endif

    // ── Mouse (desktop) — click-only semantics ─
    if (allow_mouse_input &&
        input.active_source != InputSource::Touch &&
        platform_state::input_mouse_left_released()) {
        const glm::vec2 point = to_virtual_point(
            static_cast<float>(platform_state::input_mouse_x()),
            static_cast<float>(platform_state::input_mouse_y()),
            st);
        note_pointer_click(input, point);
    }

    // ── Touch (mobile / web) — only when no mouse gesture is active ─
    if (allow_touch_input &&
        input.active_source != InputSource::Mouse &&
        platform_state::input_touch_point_count() > 0) {
        float touch_x = 0.0f;
        float touch_y = 0.0f;
        if (platform_state::input_touch_position(0, touch_x, touch_y)) {
            const glm::vec2 touch_point = to_virtual_point(touch_x, touch_y, st);
            if (!input.touching) {
                begin_pointer_contact(input, touch_point, InputSource::Touch);
            } else if (input.active_source == InputSource::Touch) {
                input.curr_x = touch_point.x;
                input.curr_y = touch_point.y;
            }
        }
    } else if (allow_touch_input &&
               input.active_source != InputSource::Mouse &&
               input.touching && input.active_source == InputSource::Touch) {
        input.touch_up  = true;
        input.end_x = input.curr_x;
        input.end_y = input.curr_y;
        input.touching = false;
        input.active_source = InputSource::None;
    }

#ifdef PLATFORM_HAS_KEYBOARD
    // ── Keyboard — IsKeyPressed fires once per press (no repeat) ──
    enqueue_go_if_pressed(disp, SDL_SCANCODE_W, SDL_SCANCODE_UP, Direction::Up);
    enqueue_go_if_pressed(disp, SDL_SCANCODE_S, SDL_SCANCODE_DOWN, Direction::Down);
    enqueue_go_if_pressed(disp, SDL_SCANCODE_A, SDL_SCANCODE_LEFT, Direction::Left);
    enqueue_go_if_pressed(disp, SDL_SCANCODE_D, SDL_SCANCODE_RIGHT, Direction::Right);

    // Keyboard shape-button presses: encode semantic payload directly — no
    // entity lookup needed (#273).
    enqueue_shape_press_if_pressed(disp, SDL_SCANCODE_1, SDL_SCANCODE_Z, Shape::Circle);
    enqueue_shape_press_if_pressed(disp, SDL_SCANCODE_2, SDL_SCANCODE_X, Shape::Square);
    enqueue_shape_press_if_pressed(disp, SDL_SCANCODE_3, SDL_SCANCODE_C, Shape::Triangle);
    if (platform_state::input_key_pressed(SDL_SCANCODE_RETURN) ||
        platform_state::input_key_pressed(SDL_SCANCODE_SPACE)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                       MenuActionKind::Confirm, 0});
    }
#endif

    // ── Background / suspend (edge-triggered) ─────────────
    // Pause only on the frame focus is *lost*, not every frame while unfocused.
    {
        bool focused = platform_state::input_window_focused();
        auto& gs = registry_ctx_get<GameState>(reg);
        if (input.was_focused && !focused && is_playing_phase(gs.phase)) {
            request_phase_transition(gs, GamePhase::Paused);
        }
        input.was_focused = focused;
    }

    if (input.touching) {
        input.duration += raw_dt;
    }

    // Touch/mouse gesture → InputEvent enqueued to dispatcher; delivered to
    // swipe routing handler via
    // disp.update<InputEvent>() in game_loop_frame (#333).
    if (input.click) {
        const auto& gs = registry_ctx_get<GameState>(reg);
        if (is_menu_phase(gs.phase)) {
            disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                            MenuActionKind::Confirm, 0});
        }
        const InputEvent event{
            InputType::Tap, Direction::Up, input.end_x, input.end_y
        };
        disp.enqueue<InputEvent>(event);
        input_latency_note_input_event_enqueued(reg, event.type, event.dir, 0);
    } else if (input.touch_up) {
        const std::uint32_t source_timestamp_ms = platform_state::input_last_gesture_timestamp_ms();
        const InputEvent event = input_event_from_detected_gesture(
            platform_state::input_read_detected_gesture(),
            input.start_y,
            input.end_x,
            input.end_y);
        disp.enqueue<InputEvent>(event);
        input_latency_note_input_event_enqueued(
            reg,
            event.type,
            event.dir,
            source_timestamp_ms
        );
    }
}
