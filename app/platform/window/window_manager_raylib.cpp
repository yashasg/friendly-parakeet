#include "window_manager.h"

#include <raylib.h>

namespace platform::window {

namespace {

class RaylibWindowManager final : public WindowManager {
public:
    void set_resizable_flag() override {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    }

    void init_window(int width, int height, const char* title) override {
        InitWindow(width, height, title);
    }

    [[nodiscard]] int current_monitor() const override {
        return GetCurrentMonitor();
    }

    [[nodiscard]] int monitor_height(int monitor_index) const override {
        return GetMonitorHeight(monitor_index);
    }

    [[nodiscard]] int monitor_width(int monitor_index) const override {
        return GetMonitorWidth(monitor_index);
    }

    void set_window_size(int width, int height) override {
        SetWindowSize(width, height);
    }

    void set_window_position(int x, int y) override {
        SetWindowPosition(x, y);
    }

    void set_target_fps(int target_fps) override {
        SetTargetFPS(target_fps);
    }

    [[nodiscard]] int screen_width() const override {
        return GetScreenWidth();
    }

    [[nodiscard]] int screen_height() const override {
        return GetScreenHeight();
    }

    [[nodiscard]] bool should_close() const override {
        return WindowShouldClose();
    }

    void close_window() override {
        CloseWindow();
    }
};

}  // namespace

WindowManager& raylib_window_manager() {
    static RaylibWindowManager instance;
    return instance;
}

WindowManager& window_manager() {
    return raylib_window_manager();
}

}  // namespace platform::window
