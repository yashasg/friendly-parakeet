#pragma once

namespace platform::sdl2 {

struct WindowConfig {
    int width = 0;
    int height = 0;
    const char* title = "";
    bool resizable = false;
};

bool create_window_and_gl_context(const WindowConfig& config);
void destroy_window_and_gl_context();
void poll_events();
[[nodiscard]] bool should_close() noexcept;
void request_close() noexcept;
[[nodiscard]] bool input_mouse_left_released() noexcept;
[[nodiscard]] int input_mouse_x() noexcept;
[[nodiscard]] int input_mouse_y() noexcept;
[[nodiscard]] bool input_key_pressed(int scancode) noexcept;
[[nodiscard]] bool input_window_focused() noexcept;

void set_window_size(int width, int height);
void set_window_position(int x, int y);
[[nodiscard]] int screen_width() noexcept;
[[nodiscard]] int screen_height() noexcept;
[[nodiscard]] int current_monitor() noexcept;
[[nodiscard]] int monitor_width(int monitor_index) noexcept;
[[nodiscard]] int monitor_height(int monitor_index) noexcept;

void set_target_fps(int target_fps) noexcept;
[[nodiscard]] int target_fps() noexcept;

void swap_window();
[[nodiscard]] float consume_frame_time() noexcept;
[[nodiscard]] bool is_ready() noexcept;

}  // namespace platform::sdl2
