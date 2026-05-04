#include "input_handler.h"

namespace platform::input {

namespace {

class Sdl2InputHandler final : public InputHandler {
public:
    void configure_gameplay_gestures() override {}
    [[nodiscard]] bool is_mouse_left_released() const override { return false; }
    [[nodiscard]] PointerPosition mouse_position() const override { return PointerPosition{}; }
    [[nodiscard]] int touch_point_count() const override { return 0; }
    [[nodiscard]] PointerPosition touch_position(int /*index*/) const override { return PointerPosition{}; }
    [[nodiscard]] bool is_key_pressed(KeyCode /*key*/) const override { return false; }
    [[nodiscard]] bool is_window_focused() const override { return true; }
    [[nodiscard]] int read_detected_gesture() const override { return 0; }
};

}  // namespace

InputHandler& sdl2_input_handler() {
    static Sdl2InputHandler instance;
    return instance;
}

}  // namespace platform::input
