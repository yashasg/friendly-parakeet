#include "platform_state.h"

#include "../util/gesture.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <utility>
#if defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#endif

namespace platform_state {

namespace {

struct GraphicsState {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool should_close = false;
    bool window_focused = true;
    bool mouse_left_released = false;
    bool mouse_left_down = false;
    int mouse_x = 0;
    int mouse_y = 0;
    std::array<bool, SDL_NUM_SCANCODES> key_pressed{};
    struct GestureSlot {
        bool active = false;
        std::int64_t finger_id = 0;
        float start_x = 0.0f;
        float start_y = 0.0f;
        std::uint32_t down_timestamp_ms = 0;
    };
    std::array<GestureSlot, 2> gesture_slots{};
    bool gestures_enabled = false;
    int detected_gesture = GESTURE_NONE;
    std::uint32_t last_gesture_timestamp_ms = 0;
    int screen_width = 0;
    int screen_height = 0;
    int target_fps = 60;
    Uint64 last_counter = 0;
};

bool g_initialized = false;
std::string g_last_error;

void capture_sdl_error() {
    g_last_error = SDL_GetError();
}

GraphicsState& state() {
    static GraphicsState instance;
    return instance;
}

void reset_input_tracking(GraphicsState& state);

void destroy_window_and_gl_context_internal() {
    auto& s = state();
    if (s.renderer) {
        SDL_DestroyRenderer(s.renderer);
        s.renderer = nullptr;
    }
    if (s.window) {
        SDL_DestroyWindow(s.window);
        s.window = nullptr;
    }
    reset_input_tracking(s);
    s.screen_width = 0;
    s.screen_height = 0;
    s.last_counter = 0;
    SDL_StopTextInput();
}

void reset_input_tracking(GraphicsState& state) {
    state.should_close = false;
    state.window_focused = true;
    state.mouse_left_released = false;
    state.mouse_left_down = false;
    state.mouse_x = 0;
    state.mouse_y = 0;
    std::fill(state.key_pressed.begin(), state.key_pressed.end(), false);
    state.gesture_slots = {};
    state.gestures_enabled = false;
    state.detected_gesture = GESTURE_NONE;
    state.last_gesture_timestamp_ms = 0;
}

void update_mouse_position_from_event(GraphicsState& state, const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION) {
        state.mouse_x = event.motion.x;
        state.mouse_y = event.motion.y;
        return;
    }
    state.mouse_x = event.button.x;
    state.mouse_y = event.button.y;
}

[[nodiscard]] std::pair<int, int> touch_coordinate_bounds(const GraphicsState& state) noexcept {
    int width = state.screen_width;
    int height = state.screen_height;
    if ((width <= 0 || height <= 0) && state.window) {
        SDL_GetWindowSize(state.window, &width, &height);
    }
    width = std::max(width, 1);
    height = std::max(height, 1);
    return {width, height};
}

GraphicsState::GestureSlot* find_gesture_slot(GraphicsState& state, std::int64_t finger_id) {
    for (auto& slot : state.gesture_slots) {
        if (slot.active && slot.finger_id == finger_id) return &slot;
    }
    return nullptr;
}

GraphicsState::GestureSlot* first_free_gesture_slot(GraphicsState& state) {
    for (auto& slot : state.gesture_slots) {
        if (!slot.active) return &slot;
    }
    return nullptr;
}

int classify_gesture(const GraphicsState::GestureSlot& slot,
                     float end_x,
                     float end_y,
                     std::uint32_t release_timestamp_ms) {
    constexpr float kMinSwipeDistance = 50.0f;
    constexpr float kMaxSwipeTimeSeconds = 0.3f;
    const float dx = end_x - slot.start_x;
    const float dy = end_y - slot.start_y;
    const float distance = std::sqrt((dx * dx) + (dy * dy));
    const std::uint32_t duration_ms =
        (release_timestamp_ms >= slot.down_timestamp_ms)
            ? (release_timestamp_ms - slot.down_timestamp_ms)
            : 0;
    const float duration_sec = static_cast<float>(duration_ms) / 1000.0f;

    if (distance >= kMinSwipeDistance && duration_sec <= kMaxSwipeTimeSeconds) {
        if (std::fabs(dx) >= std::fabs(dy)) {
            return (dx >= 0.0f) ? GESTURE_SWIPE_RIGHT : GESTURE_SWIPE_LEFT;
        }
        return (dy >= 0.0f) ? GESTURE_SWIPE_DOWN : GESTURE_SWIPE_UP;
    }
    return GESTURE_TAP;
}

}  // namespace

bool initialize() {
    if (g_initialized) return true;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        capture_sdl_error();
        return false;
    }
    g_last_error.clear();
    g_initialized = true;
    return true;
}

void shutdown() {
    if (!g_initialized) return;
    destroy_window_and_gl_context_internal();
    SDL_Quit();
    g_initialized = false;
    g_last_error.clear();
}

[[nodiscard]] const char* last_error() noexcept {
    return g_last_error.c_str();
}

bool create_window_and_gl_context(const WindowConfig& config) {
    auto& s = state();
    destroy_window_and_gl_context_internal();

    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (config.resizable) flags |= SDL_WINDOW_RESIZABLE;

    s.window = SDL_CreateWindow(config.title,
                                SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED,
                                config.width,
                                config.height,
                                flags);
    if (!s.window) {
        capture_sdl_error();
        return false;
    }

    s.renderer = SDL_CreateRenderer(
        s.window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!s.renderer) {
        s.renderer = SDL_CreateRenderer(
            s.window,
            -1,
            SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE);
    }
    if (!s.renderer) {
        capture_sdl_error();
        destroy_window_and_gl_context_internal();
        return false;
    }
    SDL_SetRenderDrawBlendMode(s.renderer, SDL_BLENDMODE_BLEND);

    reset_input_tracking(s);
    s.screen_width = config.width;
    s.screen_height = config.height;
    s.last_counter = SDL_GetPerformanceCounter();
    SDL_StartTextInput();
    g_last_error.clear();
    return true;
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
        if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
            s.mouse_left_released = true;
            update_mouse_position_from_event(s, event);
            continue;
        }
        if (event.type == SDL_MOUSEMOTION) {
            update_mouse_position_from_event(s, event);
            continue;
        }
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            update_mouse_position_from_event(s, event);
            if (event.button.button == SDL_BUTTON_LEFT) {
                s.mouse_left_down = true;
            }
            continue;
        }
        if (event.type == SDL_FINGERDOWN ||
            event.type == SDL_FINGERMOTION ||
            event.type == SDL_FINGERUP) {
            const auto [width, height] = touch_coordinate_bounds(s);
            const float px = event.tfinger.x * static_cast<float>(width);
            const float py = event.tfinger.y * static_cast<float>(height);
            const auto finger_id = static_cast<std::int64_t>(event.tfinger.fingerId);
            const auto ts_ms = static_cast<std::uint32_t>(event.tfinger.timestamp);

            if (event.type == SDL_FINGERDOWN) {
                auto* slot = find_gesture_slot(s, finger_id);
                if (!slot) {
                    slot = first_free_gesture_slot(s);
                }
                if (slot) {
                    slot->active = true;
                    slot->finger_id = finger_id;
                    slot->start_x = px;
                    slot->start_y = py;
                    slot->down_timestamp_ms = ts_ms;
                }
            } else {
                auto* slot = find_gesture_slot(s, finger_id);
                if (slot) {
                    if (s.gestures_enabled) {
                        s.detected_gesture = classify_gesture(*slot, px, py, ts_ms);
                        s.last_gesture_timestamp_ms = ts_ms;
                    }
                    *slot = {};
                }
            }
        }
    }
    const Uint32 mouse_state = SDL_GetMouseState(&s.mouse_x, &s.mouse_y);
    const bool mouse_left_current_down = (mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    s.mouse_left_released = s.mouse_left_released ||
                            (!mouse_left_current_down && s.mouse_left_down);
    s.mouse_left_down = mouse_left_current_down;
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

[[nodiscard]] bool input_window_focused() noexcept {
    return state().window_focused;
}

[[nodiscard]] int input_touch_point_count() noexcept {
    int total = 0;
    const int device_count = SDL_GetNumTouchDevices();
    for (int i = 0; i < device_count; ++i) {
        const SDL_TouchID touch_id = SDL_GetTouchDevice(i);
        const int finger_count = SDL_GetNumTouchFingers(touch_id);
        if (finger_count > 0) total += finger_count;
    }
    return total;
}

[[nodiscard]] bool input_touch_position(int index, float& x, float& y) noexcept {
    if (index < 0) return false;
    auto& s = state();
    const auto [width, height] = touch_coordinate_bounds(s);
    int active_index = 0;
    const int device_count = SDL_GetNumTouchDevices();
    for (int i = 0; i < device_count; ++i) {
        const SDL_TouchID touch_id = SDL_GetTouchDevice(i);
        const int finger_count = SDL_GetNumTouchFingers(touch_id);
        for (int f = 0; f < finger_count; ++f) {
            const SDL_Finger* finger = SDL_GetTouchFinger(touch_id, f);
            if (!finger) continue;
            if (active_index == index) {
                x = finger->x * static_cast<float>(width);
                y = finger->y * static_cast<float>(height);
                return true;
            }
            ++active_index;
        }
    }
    return false;
}

[[nodiscard]] int input_read_detected_gesture() noexcept {
    auto& s = state();
    const int detected = s.detected_gesture;
    s.detected_gesture = GESTURE_NONE;
    return detected;
}

[[nodiscard]] std::uint32_t input_last_gesture_timestamp_ms() noexcept {
    return state().last_gesture_timestamp_ms;
}

void input_configure_gameplay_gestures() noexcept {
    auto& s = state();
    s.gestures_enabled = true;
    s.detected_gesture = GESTURE_NONE;
    s.last_gesture_timestamp_ms = 0;
}

void query_display_size(float& out_w, float& out_h) noexcept {
#if defined(__EMSCRIPTEN__)
    double css_w = 0.0;
    double css_h = 0.0;
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);
    out_w = static_cast<float>(std::max(1.0, css_w));
    out_h = static_cast<float>(std::max(1.0, css_h));

    int cur_w = 0;
    int cur_h = 0;
    emscripten_get_canvas_element_size("#canvas", &cur_w, &cur_h);
    const int target_w = static_cast<int>(out_w);
    const int target_h = static_cast<int>(out_h);
    if (cur_w != target_w || cur_h != target_h) {
        emscripten_set_canvas_element_size("#canvas", target_w, target_h);
    }
    state().screen_width = target_w;
    state().screen_height = target_h;
#else
    out_w = static_cast<float>(screen_width());
    out_h = static_cast<float>(screen_height());
#endif
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

void sync_framebuffer_viewport() noexcept {
    if (!is_ready()) return;
    SDL_RenderSetViewport(state().renderer, nullptr);
}

void swap_window() {
    if (!is_ready()) return;
    SDL_RenderPresent(state().renderer);
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
    return state().window != nullptr && state().renderer != nullptr;
}

[[nodiscard]] SDL_Renderer* renderer() noexcept {
    return state().renderer;
}

}  // namespace platform_state
