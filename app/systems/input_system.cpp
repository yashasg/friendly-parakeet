#include "all_systems.h"
#include "../components/input.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include "../platform.h"
#include <raylib.h>
#include <cmath>

void input_system(entt::registry& reg, float raw_dt) {
    auto& input = reg.ctx().get<InputState>();
    auto& st    = reg.ctx().get<ScreenTransform>();
    auto& aq    = reg.ctx().get<ActionQueue>();
    aq.clear();
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
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))       aq.go(Direction::Up);
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))     aq.go(Direction::Down);
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))     aq.go(Direction::Left);
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT))    aq.go(Direction::Right);
    if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_Z))      aq.tap(Button::ShapeCircle);
    if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_X))      aq.tap(Button::ShapeSquare);
    if (IsKeyPressed(KEY_THREE) || IsKeyPressed(KEY_C))    aq.tap(Button::ShapeTri);
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) aq.tap(Button::Confirm);
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
            // Button zone (bottom 20%)
            const float btn_w       = constants::BUTTON_W_N      * constants::SCREEN_W;
            const float btn_h       = constants::BUTTON_H_N      * constants::SCREEN_H;
            const float btn_spacing = constants::BUTTON_SPACING_N * constants::SCREEN_W;
            const float btn_y       = constants::BUTTON_Y_N       * constants::SCREEN_H;
            float btn_area_x_start  = (constants::SCREEN_W - 3.0f * btn_w - 2.0f * btn_spacing) / 2.0f;
            float btn_cy            = btn_y + btn_h / 2.0f;
            float btn_radius        = btn_w / 2.8f;
            float hit_radius        = btn_radius * 1.4f;

            for (int i = 0; i < 3; ++i) {
                float btn_cx = btn_area_x_start
                    + static_cast<float>(i) * (btn_w + btn_spacing)
                    + btn_w / 2.0f;
                float dx = input.end_x - btn_cx;
                float dy = input.end_y - btn_cy;
                if (dx * dx + dy * dy <= hit_radius * hit_radius) {
                    aq.tap(static_cast<Button>(i));
                    break;
                }
            }
        } else {
            // Swipe zone
            float dx = input.end_x - input.start_x;
            float dy = input.end_y - input.start_y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= constants::MIN_SWIPE_DIST && input.duration <= constants::MAX_SWIPE_TIME) {
                if (std::abs(dx) > std::abs(dy)) {
                    aq.go(dx > 0 ? Direction::Right : Direction::Left);
                } else {
                    aq.go(dy > 0 ? Direction::Down : Direction::Up);
                }
            } else {
                aq.tap(Button::Position, input.end_x, input.end_y);
            }
        }
    }
}
