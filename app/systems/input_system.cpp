#include "all_systems.h"
#include "../components/input.h"
#include "../components/input_events.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include <raylib.h>
#include <cmath>

void input_system(entt::registry& reg, float raw_dt) {
    auto& input = reg.ctx().get<InputState>();
    auto& st    = reg.ctx().get<ScreenTransform>();
    auto& eq    = reg.ctx().get<EventQueue>();
    eq.clear();
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
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    { eq.push_go(Direction::Up); }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  { eq.push_go(Direction::Down); }
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  { eq.push_go(Direction::Left); }
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) { eq.push_go(Direction::Right); }

    auto phase = reg.ctx().get<GameState>().phase;
    auto push_active_shape_press = [&](Shape target_shape) {
        auto sv = reg.view<ShapeButtonTag, ShapeButtonData, ActiveInPhase>();
        for (auto [e, sbd, aip] : sv.each()) {
            if (sbd.shape == target_shape && phase_active(aip, phase)) {
                eq.push_press(e);
                break;
            }
        }
    };

    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_Z)) {
        push_active_shape_press(Shape::Circle);
    }
    if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_X)) {
        push_active_shape_press(Shape::Square);
    }
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_C)) {
        push_active_shape_press(Shape::Triangle);
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        auto mv = reg.view<MenuButtonTag, MenuAction, ActiveInPhase>();
        for (auto [e, ma, aip] : mv.each()) {
            if (ma.kind == MenuActionKind::Confirm && phase_active(aip, phase)) {
                eq.push_press(e);
                break;
            }
        }
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

    // Touch/mouse gesture → actions
    if (input.touch_up) {
        float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

        if (input.start_y >= zone_y) {
            // Button zone (bottom 20%) — always a Tap for the hit_test_system
            eq.push_input(InputType::Tap, input.end_x, input.end_y);
        } else {
            // Swipe zone
            float dx = input.end_x - input.start_x;
            float dy = input.end_y - input.start_y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= constants::MIN_SWIPE_DIST && input.duration <= constants::MAX_SWIPE_TIME) {
                Direction dir;
                if (std::abs(dx) > std::abs(dy)) {
                    dir = dx > 0 ? Direction::Right : Direction::Left;
                } else {
                    dir = dy > 0 ? Direction::Down : Direction::Up;
                }
                eq.push_input(InputType::Swipe, input.end_x, input.end_y, dir);
            } else {
                eq.push_input(InputType::Tap, input.end_x, input.end_y);
            }
        }
    }
}
