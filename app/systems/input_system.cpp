#include "all_systems.h"
#include "../input/gesture_input.h"
#include "../input/input_latency_probe.h"
#include "../input/input_state.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include "../platform/sdl2/sdl2_graphics_context.h"
#include "../platform/sdl2/sdl2_headers.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {

struct WebInputPolicy {
    bool prefers_touch = false;
};

bool is_menu_phase(GamePhase phase) {
    switch (phase) {
        case GamePhase::Title:
        case GamePhase::LevelSelect:
        case GamePhase::Paused:
        case GamePhase::GameOver:
        case GamePhase::SongComplete:
        case GamePhase::Settings:
        case GamePhase::Tutorial:
            return true;
        case GamePhase::Playing:
            return false;
    }
    return false;
}

} // namespace

void input_system_init(entt::registry& reg) {
    if (!reg.ctx().find<InputLatencyProbe>()) {
        reg.ctx().emplace<InputLatencyProbe>();
    }
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
    input_latency_begin_frame(reg);
    auto& input = reg.ctx().get<InputState>();
    auto& st    = reg.ctx().get<ScreenTransform>();
    auto& disp  = reg.ctx().get<entt::dispatcher>();
    platform::sdl2::poll_events();
    if (!input.gestures_configured) {
        platform::sdl2::input_configure_gameplay_gestures();
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
        platform::sdl2::input_mouse_left_released()) {
        input.click      = true;
        input.touching   = false;
        input.active_source = InputSource::None;
        const Vector2 mouse_position{
            static_cast<float>(platform::sdl2::input_mouse_x()),
            static_cast<float>(platform::sdl2::input_mouse_y())
        };
        const Vector2 pos = screen_to_virtual({mouse_position.x, mouse_position.y}, st);
        input.start_x = input.curr_x = input.end_x = pos.x;
        input.start_y = input.curr_y = input.end_y = pos.y;
        input.duration = 0.0f;
    }

    // ── Touch (mobile / web) — only when no mouse gesture is active ─
    if (allow_touch_input &&
        input.active_source != InputSource::Mouse &&
        platform::sdl2::input_touch_point_count() > 0) {
        const Vector2 touch_position{
            platform::sdl2::input_touch_x(0),
            platform::sdl2::input_touch_y(0)
        };
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
    if (platform::sdl2::input_key_pressed(SDL_SCANCODE_W) ||
        platform::sdl2::input_key_pressed(SDL_SCANCODE_UP)) {
        disp.enqueue<GoEvent>({Direction::Up});
    }
    if (platform::sdl2::input_key_pressed(SDL_SCANCODE_S) ||
        platform::sdl2::input_key_pressed(SDL_SCANCODE_DOWN)) {
        disp.enqueue<GoEvent>({Direction::Down});
    }
    if (platform::sdl2::input_key_pressed(SDL_SCANCODE_A) ||
        platform::sdl2::input_key_pressed(SDL_SCANCODE_LEFT)) {
        disp.enqueue<GoEvent>({Direction::Left});
    }
    if (platform::sdl2::input_key_pressed(SDL_SCANCODE_D) ||
        platform::sdl2::input_key_pressed(SDL_SCANCODE_RIGHT)) {
        disp.enqueue<GoEvent>({Direction::Right});
    }

    // Keyboard shape-button presses: encode semantic payload directly — no
    // entity lookup needed (#273).
    if (platform::sdl2::input_key_pressed(SDL_SCANCODE_1) ||
        platform::sdl2::input_key_pressed(SDL_SCANCODE_Z)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Circle,
                                       MenuActionKind::Confirm, 0});
    }
    if (platform::sdl2::input_key_pressed(SDL_SCANCODE_2) ||
        platform::sdl2::input_key_pressed(SDL_SCANCODE_X)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Square,
                                       MenuActionKind::Confirm, 0});
    }
    if (platform::sdl2::input_key_pressed(SDL_SCANCODE_3) ||
        platform::sdl2::input_key_pressed(SDL_SCANCODE_C)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Triangle,
                                       MenuActionKind::Confirm, 0});
    }
    if (platform::sdl2::input_key_pressed(SDL_SCANCODE_RETURN) ||
        platform::sdl2::input_key_pressed(SDL_SCANCODE_SPACE)) {
        disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                       MenuActionKind::Confirm, 0});
    }
#endif

    // ── Background / suspend (edge-triggered) ─────────────
    // Pause only on the frame focus is *lost*, not every frame while unfocused.
    {
        bool focused = platform::sdl2::input_window_focused();
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
        const auto& gs = reg.ctx().get<GameState>();
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
        const std::uint32_t source_timestamp_ms = platform::sdl2::input_last_gesture_timestamp_ms();
        const InputEvent event = input_event_from_detected_gesture(
            platform::sdl2::input_read_detected_gesture(),
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
