#include "game_loop.h"
#include "systems/test_player.h"
#include "util/level_content_config.h"

#include <cstdio>
#include <cstring>

namespace {

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
    SkillConfig test_skill = SKILL_PRO;
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
            if (std::strcmp(argv[i], "pro") == 0)       test_skill = SKILL_PRO;
            else if (std::strcmp(argv[i], "good") == 0)  test_skill = SKILL_GOOD;
            else if (std::strcmp(argv[i], "bad") == 0)   test_skill = SKILL_BAD;
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
            const char* value = argv[i];
            selected_level = -1;
            if (value[0] >= '1' && value[0] < static_cast<char>('1' + content_config::LEVEL_COUNT)
                && value[1] == '\0') {
                selected_level = value[0] - '1';
            } else {
                for (int j = 0; j < content_config::LEVEL_COUNT; ++j) {
                    if (std::strcmp(value, content_config::LEVEL_KEYS[j]) == 0 ||
                        std::strcmp(value, content_config::LEVELS[j].title) == 0) {
                        selected_level = j;
                        break;
                    }
                }
            }
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
