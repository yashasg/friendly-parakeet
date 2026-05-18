#include "test_player_session.h"
#include "../components/game_state.h"
#include "../util/app_dir_path.h"
#include "../util/level_content_config.h"
#include "../components/obstacle.h"
#include "../components/scoring.h"
#include "session_logger_system.h"

#include <raylib.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <string>

namespace {
struct TestPlayerSessionSignals {
    bool wired = false;
};
}

void test_player_init(entt::registry& reg, SkillConfig skill,
                      const char* difficulty,
                      int selected_level) {
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = content_config::level_index_or_default(selected_level);

    int sel_diff = content_config::DEFAULT_DIFFICULTY_INDEX;
    for (int d = 0; d < content_config::DIFFICULTY_COUNT; ++d) {
        if (std::strcmp(content_config::DIFFICULTY_KEYS[d], difficulty) == 0) {
            sel_diff = d;
            break;
        }
    }
    lss.selected_difficulty = sel_diff;

    auto* session_state = reg.ctx().find<TestPlayerSessionState>();
    if (!session_state) {
        session_state = &reg.ctx().emplace<TestPlayerSessionState>();
    }

    auto& tp_state = reg.ctx().get<TestPlayerState>();
    tp_state = TestPlayerState{};
    tp_state.skill  = skill;
    tp_state.active = true;

    unsigned seed = session_state->rng_seed;
    if (session_state->use_runtime_seed) {
        seed = static_cast<unsigned>(std::time(nullptr));
        session_state->rng_seed = seed;
    }
    tp_state.rng.seed(seed);

    const char* level_key = content_config::LEVEL_KEYS[lss.selected_level];
    const char* difficulty_key = content_config::DIFFICULTY_KEYS[lss.selected_difficulty];
    TraceLog(LOG_INFO, "TEST PLAYER: skill=%s level=%s difficulty=%s",
             skill.name, level_key, difficulty_key);

    auto& slog = reg.ctx().get<SessionLog>();
    session_log_close(slog);
    slog = SessionLog{};
    const double runtime_seconds = GetTime();
    const auto runtime_millis = static_cast<unsigned long long>(runtime_seconds * 1000.0);
    const uint32_t sequence = ++session_state->log_sequence;
    char log_basename[256];
    std::snprintf(log_basename, sizeof(log_basename),
        "session_%s_%s_%s_rt%010llu_n%04u.log",
        skill.name,
        level_key,
        difficulty_key,
        runtime_millis, sequence);
    const std::string log_filename =
        util::join_app_dir(GetApplicationDirectory(), log_basename);
    session_log_open(slog, log_filename.c_str());
    if (slog.file) {
        std::fprintf(slog.file, "skill=%s level=%s difficulty=%s seed=%u\n\n",
                     skill.name, level_key,
                     difficulty_key, seed);
        std::fflush(slog.file);
    }
    TraceLog(LOG_INFO, "SESSION LOG: %s", log_filename.c_str());

    auto* signals = reg.ctx().find<TestPlayerSessionSignals>();
    if (!signals) {
        signals = &reg.ctx().emplace<TestPlayerSessionSignals>();
    }
    if (!signals->wired) {
        reg.on_construct<ObstacleTag>().connect<&session_log_on_obstacle_spawn>();
        reg.on_construct<ScoredTag>().connect<&session_log_on_scored>();
        signals->wired = true;
    }
}
