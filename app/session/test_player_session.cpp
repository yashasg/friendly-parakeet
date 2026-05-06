#include "test_player_session.h"
#include "../components/game_state.h"
#include "../components/obstacle.h"
#include "../components/test_player.h"
#include "../components/registry_context.h"
#include "../util/session_logger.h"
#include <SDL.h>

#include <ctime>
#include <cstdio>

namespace {

void set_test_player_session_logging(entt::registry& reg, bool enabled) {
    reg.on_construct<ObstacleTag>().disconnect<&session_log_on_obstacle_spawn>();
    reg.on_construct<ScoredTag>().disconnect<&session_log_on_scored>();
    if (!enabled) return;
    reg.on_construct<ObstacleTag>().connect<&session_log_on_obstacle_spawn>();
    reg.on_construct<ScoredTag>().connect<&session_log_on_scored>();
}

}  // namespace

void test_player_init(entt::registry& reg, TestPlayerSkill skill,
                      const char* difficulty) {
    auto& lss = registry_ctx_get<LevelSelectState>(reg);
    lss.selected_level = clamp_level_index(1);
    const int difficulty_index = find_difficulty_index(difficulty);
    if (difficulty_index >= 0) {
        lss.selected_difficulty = clamp_difficulty_index(difficulty_index);
    }

    auto& state = registry_ctx_insert_or_assign(reg, TestPlayerState{});
    state.skill  = skill;
    state.active = true;
    state.rng.seed(static_cast<unsigned>(std::time(nullptr)));

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "TEST PLAYER: skill=%s",
                test_player_skill_name(skill));

    auto& slog = registry_ctx_get_or_emplace<SessionLog>(reg);
    session_log_close(slog);
    slog.frame = 0;
    slog.last_logged_beat = -1;
    slog.buffer.clear();
    const std::time_t now = std::time(nullptr);
    char log_filename[256];
    std::snprintf(log_filename, sizeof(log_filename),
        "session_%s_%lld.log",
        test_player_skill_name(skill),
        static_cast<long long>(now));
    session_log_open(slog, log_filename);
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SESSION LOG: %s", log_filename);

    set_test_player_session_logging(reg, true);
}

void test_player_shutdown(entt::registry& reg) {
    set_test_player_session_logging(reg, false);
    registry_ctx_if<TestPlayerState>(reg, [](TestPlayerState& state) {
        state.active = false;
        state.frame_count = 0;
        state.swipe_cooldown_timer = 0.0f;
        state.action_count = 0;
        state.planned_count = 0;
    });
}
