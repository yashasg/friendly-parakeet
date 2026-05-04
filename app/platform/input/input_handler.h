#pragma once

#include <cstdint>

namespace platform::input {

struct PointerPosition {
    float x = 0.0f;
    float y = 0.0f;
};

enum class KeyCode {
    W,
    A,
    S,
    D,
    Up,
    Down,
    Left,
    Right,
    One,
    Two,
    Three,
    Z,
    X,
    C,
    Enter,
    Space
};

class InputHandler {
public:
    virtual ~InputHandler() = default;

    virtual void pump_events() = 0;
    virtual void configure_gameplay_gestures() = 0;
    [[nodiscard]] virtual bool is_mouse_left_released() const = 0;
    [[nodiscard]] virtual PointerPosition mouse_position() const = 0;
    [[nodiscard]] virtual int touch_point_count() const = 0;
    [[nodiscard]] virtual PointerPosition touch_position(int index) const = 0;
    [[nodiscard]] virtual bool is_key_pressed(KeyCode key) const = 0;
    [[nodiscard]] virtual bool is_window_focused() const = 0;
    [[nodiscard]] virtual int read_detected_gesture() const = 0;
    [[nodiscard]] virtual std::uint32_t read_last_touch_timestamp_ms() const = 0;
};

InputHandler& input_handler();
InputHandler& raylib_input_handler();
InputHandler& sdl2_input_handler();

}  // namespace platform::input
