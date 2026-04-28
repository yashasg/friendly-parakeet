#include "game_loop.h"
#include "components/test_player.h"

#include <cstdio>
#include <cstring>

int main(int argc, char* argv[]) {

    // ── Parse CLI args ───────────────────────────────────────
    TestPlayerSkill test_skill = TestPlayerSkill::Pro;
    bool test_player_mode = false;
    const char* difficulty = "medium";
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--test-player") == 0 && i + 1 < argc) {
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
        if (std::strcmp(argv[i], "--difficulty") == 0 && i + 1 < argc) {
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
    }

    // ── Init → Run → Shutdown ────────────────────────────────
    entt::registry reg;
    game_loop_init(reg, test_player_mode, test_skill, difficulty);
    game_loop_run(reg);
    game_loop_shutdown(reg);
    return 0;
}
