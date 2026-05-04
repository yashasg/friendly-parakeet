#include "window_manager.h"
#include "../sdl2/sdl2_graphics_context.h"
#include "../sdl2/sdl2_init.h"

namespace platform::window {

namespace {

class Sdl2WindowManager final : public WindowManager {
public:
    void set_resizable_flag() override {
        resizable_ = true;
    }

    void init_window(int width, int height, const char* title) override {
        if (!platform::sdl2::initialize()) {
            platform::sdl2::request_close();
            return;
        }
        platform::sdl2::WindowConfig config{
            .width = width,
            .height = height,
            .title = title,
            .resizable = resizable_,
        };
        if (!platform::sdl2::create_window_and_gl_context(config)) {
            platform::sdl2::request_close();
        }
    }

    [[nodiscard]] int current_monitor() const override {
        return platform::sdl2::current_monitor();
    }

    [[nodiscard]] int monitor_height(int monitor_index) const override {
        return platform::sdl2::monitor_height(monitor_index);
    }

    [[nodiscard]] int monitor_width(int monitor_index) const override {
        return platform::sdl2::monitor_width(monitor_index);
    }

    void set_window_size(int width, int height) override {
        platform::sdl2::set_window_size(width, height);
    }

    void set_window_position(int x, int y) override {
        platform::sdl2::set_window_position(x, y);
    }

    void set_target_fps(int target_fps) override {
        platform::sdl2::set_target_fps(target_fps);
    }

    [[nodiscard]] int screen_width() const override {
        return platform::sdl2::screen_width();
    }

    [[nodiscard]] int screen_height() const override {
        return platform::sdl2::screen_height();
    }

    [[nodiscard]] bool should_close() const override {
        platform::sdl2::poll_events();
        return platform::sdl2::should_close();
    }

    void close_window() override {
        platform::sdl2::destroy_window_and_gl_context();
        platform::sdl2::shutdown();
    }

private:
    bool resizable_ = false;
};

}  // namespace

WindowManager& sdl2_window_manager() {
    static Sdl2WindowManager instance;
    return instance;
}

}  // namespace platform::window
