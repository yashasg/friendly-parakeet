#include "components/test_player.h"
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

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            bool ok = false;
            frames = parse_nonnegative_int(argv[++i], ok);
            if (!ok) {
                std::fprintf(stderr, "Invalid --frames value: %s\n", argv[i]);
                return 2;
            }
        } else {
            std::fprintf(stderr, "Usage: %s [--frames N]\n", argv[0]);
            return 2;
        }
    }

    SetTraceLogLevel(LOG_WARNING);

    entt::registry reg;
    game_loop_init(reg, false, TestPlayerSkill::Pro, "medium");

    float accumulator = 0.0f;
    for (int i = 0; i < frames; ++i) {
        game_loop_frame(reg, accumulator);
    }

    game_loop_shutdown(reg);
    return 0;
}
