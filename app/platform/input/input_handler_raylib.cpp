#include "input_handler.h"

#include "../../input/raylib_gesture_input.h"

#include <raylib.h>

namespace platform::input {

namespace {

class RaylibInputHandler final : public InputHandler {
public:
    void pump_events() override {}

    void configure_gameplay_gestures() override {
        SetGesturesEnabled(kGameplayGestureFlags);
    }

    [[nodiscard]] bool is_mouse_left_released() const override {
        return IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    }

    [[nodiscard]] PointerPosition mouse_position() const override {
        const Vector2 pos = GetMousePosition();
        return PointerPosition{pos.x, pos.y};
    }

    [[nodiscard]] int touch_point_count() const override {
        return GetTouchPointCount();
    }

    [[nodiscard]] PointerPosition touch_position(int index) const override {
        const Vector2 pos = GetTouchPosition(index);
        return PointerPosition{pos.x, pos.y};
    }

    [[nodiscard]] bool is_key_pressed(KeyCode key) const override {
        switch (key) {
            case KeyCode::W:     return IsKeyPressed(KEY_W);
            case KeyCode::A:     return IsKeyPressed(KEY_A);
            case KeyCode::S:     return IsKeyPressed(KEY_S);
            case KeyCode::D:     return IsKeyPressed(KEY_D);
            case KeyCode::Up:    return IsKeyPressed(KEY_UP);
            case KeyCode::Down:  return IsKeyPressed(KEY_DOWN);
            case KeyCode::Left:  return IsKeyPressed(KEY_LEFT);
            case KeyCode::Right: return IsKeyPressed(KEY_RIGHT);
            case KeyCode::One:   return IsKeyPressed(KEY_ONE);
            case KeyCode::Two:   return IsKeyPressed(KEY_TWO);
            case KeyCode::Three: return IsKeyPressed(KEY_THREE);
            case KeyCode::Z:     return IsKeyPressed(KEY_Z);
            case KeyCode::X:     return IsKeyPressed(KEY_X);
            case KeyCode::C:     return IsKeyPressed(KEY_C);
            case KeyCode::Enter: return IsKeyPressed(KEY_ENTER);
            case KeyCode::Space: return IsKeyPressed(KEY_SPACE);
        }
        return false;
    }

    [[nodiscard]] bool is_window_focused() const override {
        return IsWindowFocused();
    }

    [[nodiscard]] int read_detected_gesture() const override {
        return read_detected_raylib_gesture();
    }

    [[nodiscard]] std::uint32_t read_last_touch_timestamp_ms() const override {
        return 0;
    }
};

}  // namespace

InputHandler& raylib_input_handler() {
    static RaylibInputHandler instance;
    return instance;
}

InputHandler& input_handler() {
#if defined(SHAPESHIFTER_BACKEND_SDL2)
    return sdl2_input_handler();
#else
    return raylib_input_handler();
#endif
}

}  // namespace platform::input
