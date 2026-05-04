#include "window_manager.h"

namespace platform::window {

namespace {

class Sdl2WindowManagerStub final : public WindowManager {
public:
    void set_resizable_flag() override {}
    void init_window(int width, int height, const char* /*title*/) override {
        screen_width_ = width;
        screen_height_ = height;
    }

    [[nodiscard]] int current_monitor() const override { return 0; }
    [[nodiscard]] int monitor_height(int /*monitor_index*/) const override { return screen_height_; }
    [[nodiscard]] int monitor_width(int /*monitor_index*/) const override { return screen_width_; }

    void set_window_size(int width, int height) override {
        screen_width_ = width;
        screen_height_ = height;
    }

    void set_window_position(int /*x*/, int /*y*/) override {}
    void set_target_fps(int /*target_fps*/) override {}
    [[nodiscard]] int screen_width() const override { return screen_width_; }
    [[nodiscard]] int screen_height() const override { return screen_height_; }
    [[nodiscard]] bool should_close() const override { return false; }
    void close_window() override {}

private:
    int screen_width_ = 0;
    int screen_height_ = 0;
};

}  // namespace

WindowManager& sdl2_window_manager_stub() {
    static Sdl2WindowManagerStub instance;
    return instance;
}

}  // namespace platform::window
