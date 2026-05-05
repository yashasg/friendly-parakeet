#include "sdl2_graphics_context.h"
#include "../input/sdl2_touch_tracker.h"
#include "../../constants.h"

#include "sdl2_headers.h"
#include <algorithm>
#include <array>
#include <deque>

namespace platform::sdl2 {

namespace {

struct GraphicsState {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    bool should_close = false;
    bool window_focused = true;
    bool mouse_left_released = false;
    int mouse_x = 0;
    int mouse_y = 0;
    std::array<bool, SDL_NUM_SCANCODES> key_pressed{};
    std::deque<int> chars_pressed{};
    platform::input::Sdl2TouchTracker touch_tracker{};
    int screen_width = 0;
    int screen_height = 0;
    int target_fps = 60;
    Uint64 last_counter = 0;
};

GraphicsState& state() {
    static GraphicsState instance;
    return instance;
}

[[nodiscard]] int decode_utf8_codepoint(const char*& text) noexcept {
    if (!text || *text == '\0') return 0;
    const auto* bytes = reinterpret_cast<const unsigned char*>(text);
    const unsigned char c0 = bytes[0];
    int codepoint = 0;
    int advance = 1;
    if ((c0 & 0x80u) == 0u) {
        codepoint = c0;
    } else if ((c0 & 0xE0u) == 0xC0u && bytes[1] != '\0') {
        codepoint = ((c0 & 0x1Fu) << 6) | (bytes[1] & 0x3Fu);
        advance = 2;
    } else if ((c0 & 0xF0u) == 0xE0u && bytes[1] != '\0' && bytes[2] != '\0') {
        codepoint = ((c0 & 0x0Fu) << 12) | ((bytes[1] & 0x3Fu) << 6) | (bytes[2] & 0x3Fu);
        advance = 3;
    } else if ((c0 & 0xF8u) == 0xF0u && bytes[1] != '\0' && bytes[2] != '\0' && bytes[3] != '\0') {
        codepoint = ((c0 & 0x07u) << 18) | ((bytes[1] & 0x3Fu) << 12) |
                    ((bytes[2] & 0x3Fu) << 6) | (bytes[3] & 0x3Fu);
        advance = 4;
    } else {
        codepoint = static_cast<int>(c0);
    }
    text += advance;
    return codepoint;
}

}  // namespace

bool create_window_and_gl_context(const WindowConfig& config) {
    auto& s = state();
    destroy_window_and_gl_context();

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (config.resizable) flags |= SDL_WINDOW_RESIZABLE;

    s.window = SDL_CreateWindow(config.title,
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                config.width,
                                config.height,
                                flags);
    if (!s.window) return false;

    s.context = SDL_GL_CreateContext(s.window);
    if (!s.context) {
        destroy_window_and_gl_context();
        return false;
    }

    if (SDL_GL_MakeCurrent(s.window, s.context) != 0) {
        destroy_window_and_gl_context();
        return false;
    }
    SDL_GL_SetSwapInterval(1);

    s.should_close = false;
    s.window_focused = true;
    s.mouse_left_released = false;
    s.mouse_x = 0;
    s.mouse_y = 0;
    std::fill(s.key_pressed.begin(), s.key_pressed.end(), false);
    s.chars_pressed.clear();
    s.touch_tracker.reset();
    s.screen_width = config.width;
    s.screen_height = config.height;
    s.last_counter = SDL_GetPerformanceCounter();
    SDL_StartTextInput();
    return true;
}

void destroy_window_and_gl_context() {
    auto& s = state();
    if (s.context) {
        SDL_GL_DeleteContext(s.context);
        s.context = nullptr;
    }
    if (s.window) {
        SDL_DestroyWindow(s.window);
        s.window = nullptr;
    }
    s.should_close = false;
    s.window_focused = true;
    s.mouse_left_released = false;
    s.mouse_x = 0;
    s.mouse_y = 0;
    std::fill(s.key_pressed.begin(), s.key_pressed.end(), false);
    s.chars_pressed.clear();
    s.touch_tracker.reset();
    s.screen_width = 0;
    s.screen_height = 0;
    s.last_counter = 0;
    SDL_StopTextInput();
}

void poll_events() {
    auto& s = state();
    std::fill(s.key_pressed.begin(), s.key_pressed.end(), false);
    s.mouse_left_released = false;

    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            s.should_close = true;
            continue;
        }
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                s.should_close = true;
            } else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                s.screen_width = event.window.data1;
                s.screen_height = event.window.data2;
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                s.window_focused = true;
            } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                s.window_focused = false;
            }
            continue;
        }
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            const int scancode = static_cast<int>(event.key.keysym.scancode);
            if (scancode >= 0 && scancode < static_cast<int>(s.key_pressed.size())) {
                s.key_pressed[static_cast<size_t>(scancode)] = true;
            }
            continue;
        }
        if (event.type == SDL_TEXTINPUT) {
            const char* utf8 = event.text.text;
            while (utf8 && *utf8 != '\0') {
                const int cp = decode_utf8_codepoint(utf8);
                if (cp != 0) s.chars_pressed.push_back(cp);
            }
            continue;
        }
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            s.mouse_left_released = true;
            s.mouse_x = event.button.x;
            s.mouse_y = event.button.y;
            continue;
        }
        if (event.type == SDL_MOUSEMOTION) {
            s.mouse_x = event.motion.x;
            s.mouse_y = event.motion.y;
            continue;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            s.mouse_x = event.button.x;
            s.mouse_y = event.button.y;
            continue;
        }
        if (event.type == SDL_FINGERDOWN ||
            event.type == SDL_FINGERMOTION ||
            event.type == SDL_FINGERUP) {
            const int width = (s.screen_width > 0) ? s.screen_width : constants::SCREEN_W;
            const int height = (s.screen_height > 0) ? s.screen_height : constants::SCREEN_H;
            const float px = event.tfinger.x * static_cast<float>(width);
            const float py = event.tfinger.y * static_cast<float>(height);
            const auto finger_id = static_cast<std::int64_t>(event.tfinger.fingerId);
            const auto ts_ms = static_cast<std::uint32_t>(event.tfinger.timestamp);

            if (event.type == SDL_FINGERDOWN) {
                s.touch_tracker.on_finger_down(finger_id, px, py, ts_ms);
            } else if (event.type == SDL_FINGERMOTION) {
                s.touch_tracker.on_finger_motion(finger_id, px, py, ts_ms);
            } else {
                s.touch_tracker.on_finger_up(finger_id, px, py, ts_ms);
            }
        }
    }
    SDL_GetMouseState(&s.mouse_x, &s.mouse_y);
}

[[nodiscard]] bool should_close() noexcept {
    return state().should_close;
}

void request_close() noexcept {
#if !defined(__EMSCRIPTEN__)
    state().should_close = true;
#endif
}

[[nodiscard]] bool input_mouse_left_released() noexcept {
    return state().mouse_left_released;
}

[[nodiscard]] int input_mouse_x() noexcept {
    return state().mouse_x;
}

[[nodiscard]] int input_mouse_y() noexcept {
    return state().mouse_y;
}

[[nodiscard]] bool input_key_pressed(int scancode) noexcept {
    if (scancode < 0 || scancode >= static_cast<int>(state().key_pressed.size())) {
        return false;
    }
    return state().key_pressed[static_cast<size_t>(scancode)];
}

[[nodiscard]] int input_consume_char_pressed() noexcept {
    auto& chars = state().chars_pressed;
    if (chars.empty()) return 0;
    const int cp = chars.front();
    chars.pop_front();
    return cp;
}

[[nodiscard]] bool input_window_focused() noexcept {
    return state().window_focused;
}

[[nodiscard]] int input_touch_point_count() noexcept {
    return state().touch_tracker.touch_point_count();
}

[[nodiscard]] float input_touch_x(int index) noexcept {
    float x = 0.0f;
    float y = 0.0f;
    if (!state().touch_tracker.touch_position(index, x, y)) return 0.0f;
    return x;
}

[[nodiscard]] float input_touch_y(int index) noexcept {
    float x = 0.0f;
    float y = 0.0f;
    if (!state().touch_tracker.touch_position(index, x, y)) return 0.0f;
    return y;
}

[[nodiscard]] int input_read_detected_gesture() noexcept {
    return state().touch_tracker.consume_detected_gesture();
}

[[nodiscard]] std::uint32_t input_last_gesture_timestamp_ms() noexcept {
    return state().touch_tracker.last_gesture_timestamp_ms();
}

void input_configure_gameplay_gestures() noexcept {
    state().touch_tracker.configure_gameplay_gestures();
}

void set_window_size(int width, int height) {
    auto& s = state();
    if (!s.window) return;
    SDL_SetWindowSize(s.window, width, height);
    s.screen_width = width;
    s.screen_height = height;
}

void set_window_position(int x, int y) {
    auto& s = state();
    if (!s.window) return;
    SDL_SetWindowPosition(s.window, x, y);
}

[[nodiscard]] int screen_width() noexcept {
    return state().screen_width;
}

[[nodiscard]] int screen_height() noexcept {
    return state().screen_height;
}

[[nodiscard]] int current_monitor() noexcept {
#if defined(__EMSCRIPTEN__)
    return 0;
#else
    auto& s = state();
    if (!s.window) return 0;
    const int display = SDL_GetWindowDisplayIndex(s.window);
    return display < 0 ? 0 : display;
#endif
}

[[nodiscard]] int monitor_width(int monitor_index) noexcept {
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(monitor_index, &bounds) != 0) {
        return state().screen_width;
    }
    return bounds.w;
}

[[nodiscard]] int monitor_height(int monitor_index) noexcept {
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(monitor_index, &bounds) != 0) {
        return state().screen_height;
    }
    return bounds.h;
}

void set_target_fps(int target_fps_value) noexcept {
    state().target_fps = std::max(0, target_fps_value);
}

[[nodiscard]] int target_fps() noexcept {
    return state().target_fps;
}

void swap_window() {
    auto& s = state();
    if (!s.window) return;
    SDL_GL_SwapWindow(s.window);
}

[[nodiscard]] float consume_frame_time() noexcept {
    auto& s = state();
    if (s.last_counter == 0) return 0.0f;

    const Uint64 now = SDL_GetPerformanceCounter();
    const Uint64 delta = now - s.last_counter;
    s.last_counter = now;

    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    if (freq <= 0.0) return 0.0f;
    return static_cast<float>(static_cast<double>(delta) / freq);
}

[[nodiscard]] bool is_ready() noexcept {
    return state().window != nullptr && state().context != nullptr;
}

}  // namespace platform::sdl2
