#include "game_loop.h"
#include "components/test_player.h"
#include "content/level_content_config.h"

#include <cstdio>
#include <cstring>

namespace {

int parse_level_arg(const char* value) {
    if (value[0] >= '1' && value[0] < static_cast<char>('1' + content_config::LEVEL_COUNT)
        && value[1] == '\0') {
        return value[0] - '1';
    }
    for (int i = 0; i < content_config::LEVEL_COUNT; ++i) {
        if (std::strcmp(value, content_config::LEVEL_KEYS[i]) == 0 ||
            std::strcmp(value, content_config::LEVELS[i].title) == 0) {
            return i;
        }
    }
    return -1;
}

void print_level_help() {
    std::fprintf(stderr, "Known levels:");
    for (int i = 0; i < content_config::LEVEL_COUNT; ++i) {
        std::fprintf(stderr, " %s", content_config::LEVEL_KEYS[i]);
    }
    std::fprintf(stderr, "\n");
}

bool missing_option_value(int argc, char* argv[], int option_index) {
    return option_index + 1 >= argc || argv[option_index + 1][0] == '-';
}

}  // namespace

int main(int argc, char* argv[]) {

    // ── Parse CLI args ───────────────────────────────────────
    TestPlayerSkill test_skill = TestPlayerSkill::Pro;
    bool test_player_mode = false;
    const char* difficulty = "medium";
    int selected_level = content_config::DEFAULT_LEVEL_INDEX;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test-player") == 0) {
            if (missing_option_value(argc, argv, i)) {
                std::fprintf(stderr, "Missing value for --test-player (use pro|good|bad)\n");
                return 1;
            }
            test_player_mode = true;
            ++i;
            if (std::strcmp(argv[i], "pro") == 0)       test_skill = TestPlayerSkill::Pro;
            else if (std::strcmp(argv[i], "good") == 0)  test_skill = TestPlayerSkill::Good;
            else if (std::strcmp(argv[i], "bad") == 0)   test_skill = TestPlayerSkill::Bad;
            else {
                std::fprintf(stderr, "Unknown skill: %s (use pro|good|bad)\n", argv[i]);
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "--difficulty") == 0) {
            if (missing_option_value(argc, argv, i)) {
                std::fprintf(stderr, "Missing value for --difficulty (use easy|medium|hard)\n");
                return 1;
            }
            ++i;
            if (std::strcmp(argv[i], "easy") == 0 ||
                std::strcmp(argv[i], "medium") == 0 ||
                std::strcmp(argv[i], "hard") == 0) {
                difficulty = argv[i];
            } else {
                std::fprintf(stderr, "Unknown difficulty: %s (use easy|medium|hard)\n", argv[i]);
                return 1;
            }
        }
        else if (std::strcmp(argv[i], "--level") == 0) {
            if (missing_option_value(argc, argv, i)) {
                std::fprintf(stderr, "Missing value for --level\n");
                print_level_help();
                return 1;
            }
            ++i;
            selected_level = parse_level_arg(argv[i]);
            if (selected_level < 0) {
                std::fprintf(stderr, "Unknown level: %s\n", argv[i]);
                print_level_help();
                return 1;
            }
        }
        else {
            std::fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    // ── Init → Run → Shutdown ────────────────────────────────
    entt::registry reg;
    const bool initialized = game_loop_init(reg, test_player_mode, test_skill, difficulty, selected_level);
    if (initialized) {
        game_loop_run(reg);
    }
#ifndef __EMSCRIPTEN__
    game_loop_shutdown(reg);
#endif
    return initialized ? 0 : 1;
}
