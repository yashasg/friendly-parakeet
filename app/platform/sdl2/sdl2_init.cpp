#include "sdl2_init.h"

#include "sdl2_headers.h"
#include <string>

namespace platform::sdl2 {

namespace {

bool g_initialized = false;
std::string g_last_error;

}  // namespace

bool initialize() {
    if (g_initialized) return true;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        g_last_error = SDL_GetError();
        return false;
    }
    g_last_error.clear();
    g_initialized = true;
    return true;
}

void shutdown() {
    if (!g_initialized) return;
    SDL_Quit();
    g_initialized = false;
    g_last_error.clear();
}

[[nodiscard]] const char* last_error() noexcept {
    return g_last_error.c_str();
}

[[nodiscard]] bool is_initialized() noexcept {
    return g_initialized;
}

}  // namespace platform::sdl2
