#include "test_player_session.h"
#include "../components/game_state.h"
#include "../components/obstacle.h"
#include "../components/scoring.h"
#include "../util/session_logger.h"
#include "../util/safe_localtime.h"

#include <raylib.h>
#include <ctime>
#include <cstdio>
#include <cstring>

namespace {
struct TestPlayerSessionSignals {
    bool wired = false;
};
}

void test_player_init(entt::registry& reg, TestPlayerSkill skill,
                      const char* difficulty) {
    auto& lss = reg.ctx().get<LevelSelectState>();
    lss.selected_level = 1;
    for (int d = 0; d < 3; ++d)
        if (std::strcmp(LevelSelectState::DIFFICULTY_KEYS[d], difficulty) == 0)
            { lss.selected_difficulty = d; break; }

    auto* tp_state_ptr = reg.ctx().find<TestPlayerState>();
    if (!tp_state_ptr) {
        tp_state_ptr = &reg.ctx().emplace<TestPlayerState>();
    }
    *tp_state_ptr = TestPlayerState{};
    auto& tp_state = *tp_state_ptr;
    tp_state.skill  = skill;
    tp_state.active = true;
    tp_state.rng.seed(static_cast<unsigned>(std::time(nullptr)));

    static const char* skill_names[] = { "pro", "good", "bad" };
    TraceLog(LOG_INFO, "TEST PLAYER: skill=%s",
             skill_names[static_cast<int>(skill)]);

    auto* slog_ptr = reg.ctx().find<SessionLog>();
    if (!slog_ptr) {
        slog_ptr = &reg.ctx().emplace<SessionLog>();
    } else {
        session_log_close(*slog_ptr);
        *slog_ptr = SessionLog{};
    }
    auto& slog = *slog_ptr;
    std::time_t now = std::time(nullptr);
    std::tm tm{};
    safe_localtime(&now, &tm);
    char log_filename[256];
    std::snprintf(log_filename, sizeof(log_filename),
        "%ssession_%s_%04d%02d%02d_%02d%02d%02d.log",
        GetApplicationDirectory(),
        skill_names[static_cast<int>(skill)],
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);
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
