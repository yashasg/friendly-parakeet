#include "session_logger.h"
#include "../components/obstacle.h"
#include "../components/rhythm.h"
#include "../components/transform.h"
#include "../components/scoring.h"
#include "../components/game_state.h"
#include "../constants.h"

#include <cstdarg>
#include <cmath>
#include <string_view>

#include <magic_enum/magic_enum.hpp>
#include <raylib.h>

namespace {

template <typename E>
std::string_view enum_name_or_unknown(E value) {
    const std::string_view name = magic_enum::enum_name(value);
    return name.empty() ? std::string_view{"???"} : name;
}

} // namespace

// ── Core log function ────────────────────────────────────────

void session_log_open(SessionLog& log, const char* path) {
    if (log.file) std::fclose(log.file);
    log.file = std::fopen(path, "w");
    if (log.file) {
        std::fprintf(log.file, "══════ Test Session started runtime=%.3fs ══════\n\n", GetTime());
        std::fflush(log.file);
    }
    log.frame = 0;
}

void session_log_close(SessionLog& log) {
    session_log_flush(log);  // flush any remaining buffered lines
    if (log.file) {
        std::fclose(log.file);
        log.file = nullptr;
    }
}

void session_log_write(SessionLog& log, float song_time,
                       const char* tag, const char* fmt, ...) {
    if (!log.file) return;

    // Format header
    char header[64];
    std::snprintf(header, sizeof(header), "[F:%04u T:%06.3f] [%-6s] ",
                  log.frame, song_time, tag);
    log.buffer.append(header);

    // Format message
    va_list args;
    va_start(args, fmt);
    char msg[256];
    std::vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    log.buffer.append(msg);
    log.buffer.push_back('\n');
}

void session_log_flush(SessionLog& log) {
    if (!log.file || log.buffer.empty()) return;
    std::fwrite(log.buffer.data(), 1, log.buffer.size(), log.file);
    std::fflush(log.file);
    log.buffer.clear();  // capacity preserved — no realloc next frame
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

    int8_t lane = 1;
    auto* rlane = reg.try_get<RequiredLane>(entity);
    if (rlane) lane = rlane->lane;

    float arrival = beat ? beat->arrival_time : 0.0f;
    const std::string_view kind_name = enum_name_or_unknown(obs->kind);
    const std::string_view shape_name = req ? enum_name_or_unknown(req->shape) : std::string_view{"-"};

    session_log_write(*log, t, "GAME",
        "OBSTACLE_SPAWN beat=%d arrival=%.3f kind=%.*s shape=%.*s lane=%d",
        beat_idx, arrival,
        static_cast<int>(kind_name.size()), kind_name.data(),
        static_cast<int>(shape_name.size()), shape_name.data(),
        lane);
}

// ── EnTT signal: obstacle scored (collision resolved) ────────

void session_log_on_scored(entt::registry& reg, entt::entity entity) {
    auto* log = reg.ctx().find<SessionLog>();
    if (!log || !log->file) return;

    auto* song = reg.ctx().find<SongState>();
    float t = song ? song->song_time : 0.0f;

    auto* obs = reg.try_get<Obstacle>(entity);
    if (!obs) return;

    bool is_miss = reg.any_of<MissTag>(entity);

    auto* grade = reg.try_get<TimingGrade>(entity);
    auto* beat = reg.try_get<BeatInfo>(entity);
    int beat_num = beat ? beat->beat_index : -1;
    float expected_t = beat ? beat->arrival_time : 0.0f;
    float drift = beat ? (t - beat->arrival_time) : 0.0f;
    const std::string_view kind_name = enum_name_or_unknown(obs->kind);

    if (is_miss) {
        session_log_write(*log, t, "GAME",
            "COLLISION obstacle=%u beat=%d expected=%.3f drift=%+.3fs kind=%.*s result=MISS",
            static_cast<unsigned>(entt::to_integral(entity)),
            beat_num, expected_t, drift,
            static_cast<int>(kind_name.size()), kind_name.data());
    } else if (grade) {
        const std::string_view tier_name = enum_name_or_unknown(grade->tier);
        session_log_write(*log, t, "GAME",
            "COLLISION obstacle=%u beat=%d expected=%.3f drift=%+.3fs kind=%.*s result=CLEAR timing=%.*s(%.2f)",
            static_cast<unsigned>(entt::to_integral(entity)),
            beat_num, expected_t, drift,
            static_cast<int>(kind_name.size()), kind_name.data(),
            static_cast<int>(tier_name.size()), tier_name.data(),
            grade->precision);
    } else {
        session_log_write(*log, t, "GAME",
            "COLLISION obstacle=%u beat=%d expected=%.3f drift=%+.3fs kind=%.*s result=CLEAR",
            static_cast<unsigned>(entt::to_integral(entity)),
            beat_num, expected_t, drift,
            static_cast<int>(kind_name.size()), kind_name.data());
    }
}
