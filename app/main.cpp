#include "game_loop.h"
#include "components/test_player.h"

#include <cstdio>
#include <cstring>

namespace {

struct CliOptions {
    bool test_player_mode = false;
    TestPlayerSkill test_skill = TestPlayerSkill::Pro;
    const char* difficulty = "medium";
};

bool is_cli_difficulty_supported(const char* difficulty) {
    return difficulty &&
           (std::strcmp(difficulty, "easy") == 0 ||
            std::strcmp(difficulty, "medium") == 0 ||
            std::strcmp(difficulty, "hard") == 0);
}

bool parse_cli_options(int argc, char* argv[], CliOptions& options) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test-player") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Missing skill after --test-player (use pro|good|bad)\n");
                return false;
            }
            options.test_player_mode = true;
            ++i;
            if (!try_parse_test_player_skill(argv[i], options.test_skill)) {
                std::fprintf(stderr, "Unknown skill: %s (use pro|good|bad)\n", argv[i]);
                return false;
            }
        } else if (std::strcmp(argv[i], "--difficulty") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Missing value after --difficulty (use easy|medium|hard)\n");
                return false;
            }
            ++i;
            if (is_cli_difficulty_supported(argv[i])) {
                options.difficulty = argv[i];
            } else {
                std::fprintf(stderr, "Unknown difficulty: %s (use easy|medium|hard)\n", argv[i]);
                return false;
            }
        }
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    CliOptions options{};
    if (!parse_cli_options(argc, argv, options)) {
        return 1;
    }

    // ── Init → Run → Shutdown ────────────────────────────────
    entt::registry reg;
    if (!game_loop_init(reg, options.test_player_mode, options.test_skill, options.difficulty)) {
        return 1;
    }
    game_loop_run(reg);
#ifndef __EMSCRIPTEN__
    game_loop_shutdown(reg);
#endif
    return 0;
}
