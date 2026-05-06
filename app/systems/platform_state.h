#pragma once

#include <cstdint>
#include <SDL.h>

namespace platform_state {

bool initialize();
void shutdown();
[[nodiscard]] const char* last_error() noexcept;

struct WindowConfig {
    int width = 0;
    int height = 0;
    const char* title = "";
    bool resizable = false;
};

bool create_window_and_gl_context(const WindowConfig& config);
void poll_events();
[[nodiscard]] bool should_close() noexcept;
void request_close() noexcept;
[[nodiscard]] bool input_mouse_left_released() noexcept;
[[nodiscard]] int input_mouse_x() noexcept;
[[nodiscard]] int input_mouse_y() noexcept;
[[nodiscard]] bool input_key_pressed(int scancode) noexcept;
[[nodiscard]] bool input_window_focused() noexcept;
[[nodiscard]] int input_touch_point_count() noexcept;
[[nodiscard]] bool input_touch_position(int index, float& x, float& y) noexcept;
[[nodiscard]] int input_read_detected_gesture() noexcept;
[[nodiscard]] std::uint32_t input_last_gesture_timestamp_ms() noexcept;
void input_configure_gameplay_gestures() noexcept;
void query_display_size(float& out_w, float& out_h) noexcept;

void set_window_size(int width, int height);
void set_window_position(int x, int y);
[[nodiscard]] int screen_width() noexcept;
[[nodiscard]] int screen_height() noexcept;
[[nodiscard]] int current_monitor() noexcept;
[[nodiscard]] int monitor_width(int monitor_index) noexcept;
[[nodiscard]] int monitor_height(int monitor_index) noexcept;

void set_target_fps(int target_fps) noexcept;
[[nodiscard]] int target_fps() noexcept;

void sync_framebuffer_viewport() noexcept;
void swap_window();
[[nodiscard]] float consume_frame_time() noexcept;
[[nodiscard]] bool is_ready() noexcept;
[[nodiscard]] SDL_Renderer* renderer() noexcept;

}  // namespace platform_state
