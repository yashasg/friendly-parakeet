#include "input_handler.h"

#include "../sdl2/sdl2_graphics_context.h"

#if !defined(__EMSCRIPTEN__)
#include "../sdl2/sdl2_headers.h"
#endif

namespace platform::input {

namespace {

#if !defined(__EMSCRIPTEN__)
[[nodiscard]] int keycode_to_scancode(KeyCode key) {
    switch (key) {
        case KeyCode::W:     return SDL_SCANCODE_W;
        case KeyCode::A:     return SDL_SCANCODE_A;
        case KeyCode::S:     return SDL_SCANCODE_S;
        case KeyCode::D:     return SDL_SCANCODE_D;
        case KeyCode::Up:    return SDL_SCANCODE_UP;
        case KeyCode::Down:  return SDL_SCANCODE_DOWN;
        case KeyCode::Left:  return SDL_SCANCODE_LEFT;
        case KeyCode::Right: return SDL_SCANCODE_RIGHT;
        case KeyCode::One:   return SDL_SCANCODE_1;
        case KeyCode::Two:   return SDL_SCANCODE_2;
        case KeyCode::Three: return SDL_SCANCODE_3;
        case KeyCode::Z:     return SDL_SCANCODE_Z;
        case KeyCode::X:     return SDL_SCANCODE_X;
        case KeyCode::C:     return SDL_SCANCODE_C;
        case KeyCode::Enter: return SDL_SCANCODE_RETURN;
        case KeyCode::Space: return SDL_SCANCODE_SPACE;
    }
    return SDL_SCANCODE_UNKNOWN;
}
#endif

class Sdl2InputHandler final : public InputHandler {
public:
    void pump_events() override {
        platform::sdl2::poll_events();
    }

    void configure_gameplay_gestures() override {
        // TODO(phase4): gesture/touch migration requires a safe SDL2 recognizer.
    }

    [[nodiscard]] bool is_mouse_left_released() const override {
        return platform::sdl2::input_mouse_left_released();
    }

    [[nodiscard]] PointerPosition mouse_position() const override {
        return PointerPosition{
            static_cast<float>(platform::sdl2::input_mouse_x()),
            static_cast<float>(platform::sdl2::input_mouse_y())
        };
    }

    [[nodiscard]] int touch_point_count() const override {
        // TODO(phase4): wire SDL touch events (SDL_FINGER*) into InputState.
        return 0;
    }

    [[nodiscard]] PointerPosition touch_position(int /*index*/) const override {
        // TODO(phase4): return tracked SDL touch points.
        return PointerPosition{};
    }

    [[nodiscard]] bool is_key_pressed(KeyCode key) const override {
#if defined(__EMSCRIPTEN__)
        (void)key;
        return false;
#else
        return platform::sdl2::input_key_pressed(keycode_to_scancode(key));
#endif
    }

    [[nodiscard]] bool is_window_focused() const override {
        return platform::sdl2::input_window_focused();
    }

    [[nodiscard]] int read_detected_gesture() const override {
        // TODO(phase4): map SDL2 touch traces to gesture IDs.
        return 0;
    }
};

}  // namespace

InputHandler& sdl2_input_handler() {
    static Sdl2InputHandler instance;
    return instance;
}

}  // namespace platform::input
