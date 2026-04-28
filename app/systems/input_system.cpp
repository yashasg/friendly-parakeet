#include "all_systems.h"
#include "input_gesture.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include <raylib.h>

void input_system(entt::registry& reg, float raw_dt) {
    auto& input = reg.ctx().get<InputState>();
    auto& st    = reg.ctx().get<ScreenTransform>();
    auto& disp  = reg.ctx().get<entt::dispatcher>();
    // Discard any InputEvents that were not consumed before this frame
    // (defensive guard — R7: phase transitions can leave events queued).
    disp.clear<InputEvent>();
    clear_input_events(input);

    // Convert a raw window pixel coordinate to virtual world space.
    // At the initial 720×1280 window (scale=1, offset=0) this is a no-op;
    // when the window is resized the letterbox offsets and scale are applied.
    auto to_vx = [&](float wx) { return (wx - st.offset_x) / st.scale; };
    auto to_vy = [&](float wy) { return (wy - st.offset_y) / st.scale; };

    // ── Mouse (desktop) — only when no touch gesture is active ─
    if (input.active_source != InputSource::Touch) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            input.touch_down = true;
            input.touching   = true;
            input.active_source = InputSource::Mouse;
            Vector2 pos = GetMousePosition();
            input.start_x = input.curr_x = to_vx(pos.x);
            input.start_y = input.curr_y = to_vy(pos.y);
            input.duration = 0.0f;
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
            input.active_source == InputSource::Mouse) {
            input.touch_up  = true;
            input.touching  = false;
            input.active_source = InputSource::None;
            Vector2 pos = GetMousePosition();
            input.end_x = to_vx(pos.x);
            input.end_y = to_vy(pos.y);
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && input.touching &&
            input.active_source == InputSource::Mouse) {
            Vector2 pos = GetMousePosition();
            input.curr_x = to_vx(pos.x);
            input.curr_y = to_vy(pos.y);
        }
    }

    // ── Touch (mobile / web) — only when no mouse gesture is active ─
    if (input.active_source != InputSource::Mouse) {
        if (GetTouchPointCount() > 0) {
            Vector2 tp = GetTouchPosition(0);
            if (!input.touching) {
                input.touch_down = true;
                input.touching   = true;
                input.active_source = InputSource::Touch;
                input.start_x = input.curr_x = to_vx(tp.x);
                input.start_y = input.curr_y = to_vy(tp.y);
                input.duration = 0.0f;
            } else if (input.active_source == InputSource::Touch) {
                input.curr_x = to_vx(tp.x);
                input.curr_y = to_vy(tp.y);
            }
        } else if (input.touching && input.active_source == InputSource::Touch) {
            input.touch_up  = true;
            input.touching  = false;
            input.active_source = InputSource::None;
            input.end_x = input.curr_x;
            input.end_y = input.curr_y;
        }
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
    // gesture_routing_handle_input and hit_test_handle_input via
    // disp.update<InputEvent>() in game_loop_frame (#333).
    if (input.touch_up) {
        const InputEvent event = classify_touch_release(
            input.start_x, input.start_y,
            input.end_x, input.end_y,
            input.duration
        );
        disp.enqueue<InputEvent>(event);
    }
}
