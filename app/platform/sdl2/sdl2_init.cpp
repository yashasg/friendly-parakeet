#include "sdl2_init.h"

#if !defined(__EMSCRIPTEN__)
#include "sdl2_headers.h"
#include <string>
#endif

namespace platform::sdl2 {

#if !defined(__EMSCRIPTEN__)
namespace {

bool g_initialized = false;
std::string g_last_error;

}  // namespace
#endif

bool initialize() {
#if defined(__EMSCRIPTEN__)
    return false;
#else
    if (g_initialized) return true;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        g_last_error = SDL_GetError();
        return false;
    }
    g_last_error.clear();
    g_initialized = true;
    return true;
#endif
}

void shutdown() {
#if !defined(__EMSCRIPTEN__)
    if (!g_initialized) return;
    SDL_Quit();
    g_initialized = false;
    g_last_error.clear();
#endif
}

[[nodiscard]] const char* last_error() noexcept {
#if defined(__EMSCRIPTEN__)
    return "SDL2 native bring-up is not supported on Emscripten in phase 2";
#else
    return g_last_error.c_str();
#endif
}

[[nodiscard]] bool is_initialized() noexcept {
#if defined(__EMSCRIPTEN__)
    return false;
#else
    return g_initialized;
#endif
}

}  // namespace platform::sdl2
