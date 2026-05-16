#include "systems/test_player.h"
#include "game_loop.h"

#include <raylib.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int parse_nonnegative_int(const char* text, bool& ok) {
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    ok = end && *end == '\0' && value >= 0;
    return ok ? static_cast<int>(value) : 0;
}

}  // namespace

int main(int argc, char** argv) {
    int frames = 0;
    int cycles = 1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            bool ok = false;
            frames = parse_nonnegative_int(argv[++i], ok);
            if (!ok) {
                std::fprintf(stderr, "Invalid --frames value: %s\n", argv[i]);
                return 2;
            }
        } else if (std::strcmp(argv[i], "--cycles") == 0 && i + 1 < argc) {
            bool ok = false;
            cycles = parse_nonnegative_int(argv[++i], ok);
            if (!ok || cycles == 0) {
                std::fprintf(stderr, "Invalid --cycles value: %s\n", argv[i]);
                return 2;
            }
        } else {
            std::fprintf(stderr, "Usage: %s [--frames N] [--cycles N]\n", argv[0]);
            return 2;
        }
    }

    SetTraceLogLevel(LOG_WARNING);
    const char* smoke_mode = std::getenv("SHAPESHIFTER_STARTUP_SHUTDOWN_SMOKE");
    if (smoke_mode && smoke_mode[0] != '\0' &&
        !(smoke_mode[0] == '0' && smoke_mode[1] == '\0')) {
#if defined(__APPLE__) || defined(_WIN32)
        std::fprintf(stderr, "SKIPPED: hosted runner has no reliable OpenGL window context\n");
        return 77;
#endif
#if defined(__linux__)
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
#endif
    }

    entt::registry reg;
    for (int cycle = 0; cycle < cycles; ++cycle) {
        game_loop_init(reg, false, SKILL_PRO, "medium");
        if (!IsWindowReady()) {
            std::fprintf(stderr, "SKIPPED: OpenGL window context is unavailable\n");
            return 77;
        }

        float accumulator = 0.0f;
        for (int i = 0; i < frames; ++i) {
            game_loop_frame(reg, accumulator);
        }

        game_loop_shutdown(reg);
    }
    return 0;
}
