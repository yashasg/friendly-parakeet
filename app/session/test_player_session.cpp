#include "test_player_session.h"
#include "../components/game_state.h"
#include "../content/level_content_config.h"
#include "../components/obstacle.h"
#include "../components/scoring.h"
#include "../util/session_logger.h"

#include <raylib.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace {
struct TestPlayerSessionSignals {
    bool wired = false;
};
}

void test_player_init(entt::registry& reg, TestPlayerSkill skill,
                      const char* difficulty) {
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;
    for (int d = 0; d < content_config::DIFFICULTY_COUNT; ++d)
        if (std::strcmp(content_config::DIFFICULTY_KEYS[d], difficulty) == 0)
            { lss.selected_difficulty = d; break; }

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

    static const char* skill_names[] = { "pro", "good", "bad" };
    TraceLog(LOG_INFO, "TEST PLAYER: skill=%s",
             skill_names[static_cast<int>(skill)]);

    auto& slog = reg.ctx().get<SessionLog>();
    session_log_close(slog);
    slog = SessionLog{};
    const double runtime_seconds = GetTime();
    const auto runtime_millis = static_cast<unsigned long long>(runtime_seconds * 1000.0);
    const uint32_t sequence = ++session_state->log_sequence;
    char log_filename[256];
    std::snprintf(log_filename, sizeof(log_filename),
        "%ssession_%s_rt%010llu_n%04u.log",
        GetApplicationDirectory(),
        skill_names[static_cast<int>(skill)],
        runtime_millis, sequence);
    session_log_open(slog, log_filename);
    TraceLog(LOG_INFO, "SESSION LOG: %s", log_filename);

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
