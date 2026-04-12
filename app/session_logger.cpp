#include "session_logger.h"
#include "components/obstacle.h"
#include "components/obstacle_data.h"
#include "components/rhythm.h"
#include "components/ring_zone.h"
#include "components/transform.h"
#include "components/scoring.h"
#include "components/game_state.h"
#include "enum_names.h"
#include "platform_utils.h"
#include "constants.h"

#include <cstdarg>
#include <cmath>

// ── Core log function ────────────────────────────────────────

void session_log_open(SessionLog& log, const char* path) {
    if (log.file) std::fclose(log.file);
    log.file = safe_fopen(path, "w");
    if (log.file) {
        std::time_t now = std::time(nullptr);
        std::tm tm = safe_localtime(&now);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M", &tm);
        std::fprintf(log.file, "══════ Test Session started %s ══════\n\n", ts);
        std::fflush(log.file);
    }
    log.frame = 0;
}

void session_log_close(SessionLog& log) {
    if (log.file) {
        std::fflush(log.file);
        std::fclose(log.file);
        log.file = nullptr;
    }
}

void session_log_write(SessionLog& log, float song_time,
                       const char* tag, const char* fmt, ...) {
    if (!log.file) return;

    std::fprintf(log.file, "[F:%04u T:%06.3f] [%-6s] ", log.frame, song_time, tag);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(log.file, fmt, args);
    va_end(args);

    std::fprintf(log.file, "\n");
    std::fflush(log.file);
}

// ── EnTT signal: obstacle spawned ────────────────────────────

void session_log_on_obstacle_spawn(entt::registry& reg, entt::entity entity) {
    auto* log = reg.ctx().find<SessionLog>();
    if (!log || !log->file) return;

    auto* song = reg.ctx().find<SongState>();
    float t = song ? song->song_time : 0.0f;

    auto* obs = reg.try_get<Obstacle>(entity);
    if (!obs) return;

    auto* beat = reg.try_get<BeatInfo>(entity);
    int beat_idx = beat ? beat->beat_index : -1;

    auto* req = reg.try_get<RequiredShape>(entity);
    auto* pos = reg.try_get<Position>(entity);

    int8_t lane = 1;
    auto* rlane = reg.try_get<RequiredLane>(entity);
    if (rlane) lane = rlane->lane;

    float arrival = beat ? beat->arrival_time : 0.0f;

    session_log_write(*log, t, "GAME",
        "OBSTACLE_SPAWN beat=%d arrival=%.3f kind=%s shape=%s lane=%d",
        beat_idx, arrival, obstacle_kind_name(obs->kind),
        req ? shape_name(req->shape) : "-", lane);

    // Emplace RingZoneTracker only on obstacles that have a ring visual
    if (req) {
        reg.emplace<RingZoneTracker>(entity);

        float dist = pos ? (constants::PLAYER_Y - pos->y) : constants::APPROACH_DIST;
        session_log_write(*log, t, "GAME",
            "RING_APPEAR shape=%s obstacle=%u dist=%.0fpx",
            shape_name(req->shape),
            static_cast<unsigned>(entt::to_integral(entity)), dist);
    }
}

// ── EnTT signal: obstacle scored (collision resolved) ────────

void session_log_on_scored(entt::registry& reg, entt::entity entity) {
    auto* log = reg.ctx().find<SessionLog>();
    if (!log || !log->file) return;

    auto* song = reg.ctx().find<SongState>();
    float t = song ? song->song_time : 0.0f;

    auto* obs = reg.try_get<Obstacle>(entity);
    if (!obs) return;

    auto* gs = reg.ctx().find<GameState>();
    bool is_miss = gs && gs->transition_pending &&
                   gs->next_phase == GamePhase::GameOver;

    auto* grade = reg.try_get<TimingGrade>(entity);
    auto* beat = reg.try_get<BeatInfo>(entity);
    int beat_num = beat ? beat->beat_index : -1;
    float expected_t = beat ? beat->arrival_time : 0.0f;
    float drift = beat ? (t - beat->arrival_time) : 0.0f;

    if (is_miss) {
        session_log_write(*log, t, "GAME",
            "COLLISION obstacle=%u beat=%d expected=%.3f drift=%+.3fs kind=%s result=MISS",
            static_cast<unsigned>(entt::to_integral(entity)),
            beat_num, expected_t, drift, obstacle_kind_name(obs->kind));
    } else if (grade) {
        session_log_write(*log, t, "GAME",
            "COLLISION obstacle=%u beat=%d expected=%.3f drift=%+.3fs kind=%s result=CLEAR timing=%s(%.2f)",
            static_cast<unsigned>(entt::to_integral(entity)),
            beat_num, expected_t, drift, obstacle_kind_name(obs->kind),
            timing_tier_name(grade->tier), grade->precision);
    } else {
        session_log_write(*log, t, "GAME",
            "COLLISION obstacle=%u beat=%d expected=%.3f drift=%+.3fs kind=%s result=CLEAR",
            static_cast<unsigned>(entt::to_integral(entity)),
            beat_num, expected_t, drift, obstacle_kind_name(obs->kind));
    }
}
