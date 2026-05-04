#include "sdl2_graphics_context.h"

#if !defined(__EMSCRIPTEN__)
#include "sdl2_headers.h"
#include <algorithm>
#endif

namespace platform::sdl2 {

#if !defined(__EMSCRIPTEN__)
namespace {

struct GraphicsState {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    bool should_close = false;
    int screen_width = 0;
    int screen_height = 0;
    int target_fps = 60;
    Uint64 last_counter = 0;
};

GraphicsState& state() {
    static GraphicsState instance;
    return instance;
}

}  // namespace
#endif

bool create_window_and_gl_context(const WindowConfig& config) {
#if defined(__EMSCRIPTEN__)
    (void)config;
    return false;
#else
    auto& s = state();
    destroy_window_and_gl_context();

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif
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
    s.screen_width = config.width;
    s.screen_height = config.height;
    s.last_counter = SDL_GetPerformanceCounter();
    return true;
#endif
}

void destroy_window_and_gl_context() {
#if !defined(__EMSCRIPTEN__)
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
    s.screen_width = 0;
    s.screen_height = 0;
    s.last_counter = 0;
#endif
}

void poll_events() {
#if !defined(__EMSCRIPTEN__)
    auto& s = state();
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
            }
        }
    }
#endif
}

[[nodiscard]] bool should_close() noexcept {
#if defined(__EMSCRIPTEN__)
    return true;
#else
    return state().should_close;
#endif
}

void request_close() noexcept {
#if !defined(__EMSCRIPTEN__)
    state().should_close = true;
#endif
}

void set_window_size(int width, int height) {
#if !defined(__EMSCRIPTEN__)
    auto& s = state();
    if (!s.window) return;
    SDL_SetWindowSize(s.window, width, height);
    s.screen_width = width;
    s.screen_height = height;
#else
    (void)width;
    (void)height;
#endif
}

void set_window_position(int x, int y) {
#if !defined(__EMSCRIPTEN__)
    auto& s = state();
    if (!s.window) return;
    SDL_SetWindowPosition(s.window, x, y);
#else
    (void)x;
    (void)y;
#endif
}

[[nodiscard]] int screen_width() noexcept {
#if defined(__EMSCRIPTEN__)
    return 0;
#else
    return state().screen_width;
#endif
}

[[nodiscard]] int screen_height() noexcept {
#if defined(__EMSCRIPTEN__)
    return 0;
#else
    return state().screen_height;
#endif
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
#if defined(__EMSCRIPTEN__)
    (void)monitor_index;
    return 0;
#else
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(monitor_index, &bounds) != 0) {
        return state().screen_width;
    }
    return bounds.w;
#endif
}

[[nodiscard]] int monitor_height(int monitor_index) noexcept {
#if defined(__EMSCRIPTEN__)
    (void)monitor_index;
    return 0;
#else
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(monitor_index, &bounds) != 0) {
        return state().screen_height;
    }
    return bounds.h;
#endif
}

void set_target_fps(int target_fps_value) noexcept {
#if !defined(__EMSCRIPTEN__)
    state().target_fps = std::max(0, target_fps_value);
#else
    (void)target_fps_value;
#endif
}

[[nodiscard]] int target_fps() noexcept {
#if defined(__EMSCRIPTEN__)
    return 0;
#else
    return state().target_fps;
#endif
}

void swap_window() {
#if !defined(__EMSCRIPTEN__)
    auto& s = state();
    if (!s.window) return;
    SDL_GL_SwapWindow(s.window);
#endif
}

[[nodiscard]] float consume_frame_time() noexcept {
#if defined(__EMSCRIPTEN__)
    return 0.0f;
#else
    auto& s = state();
    if (s.last_counter == 0) return 0.0f;

    const Uint64 now = SDL_GetPerformanceCounter();
    const Uint64 delta = now - s.last_counter;
    s.last_counter = now;

    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    if (freq <= 0.0) return 0.0f;
    return static_cast<float>(static_cast<double>(delta) / freq);
#endif
}

[[nodiscard]] bool is_ready() noexcept {
#if defined(__EMSCRIPTEN__)
    return false;
#else
    return state().window != nullptr && state().context != nullptr;
#endif
}

}  // namespace platform::sdl2
