#pragma once

namespace platform::window {

class WindowManager {
public:
    virtual ~WindowManager() = default;

    virtual void set_resizable_flag() = 0;
    virtual void init_window(int width, int height, const char* title) = 0;
    virtual int current_monitor() const = 0;
    virtual int monitor_height(int monitor_index) const = 0;
    virtual int monitor_width(int monitor_index) const = 0;
    virtual void set_window_size(int width, int height) = 0;
    virtual void set_window_position(int x, int y) = 0;
    virtual void set_target_fps(int target_fps) = 0;
    [[nodiscard]] virtual int screen_width() const = 0;
    [[nodiscard]] virtual int screen_height() const = 0;
    [[nodiscard]] virtual bool should_close() const = 0;
    virtual void close_window() = 0;
};

WindowManager& window_manager();
WindowManager& raylib_window_manager();
WindowManager& sdl2_window_manager();

}  // namespace platform::window
