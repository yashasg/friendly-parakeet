#include "all_systems.h"
#include "../entities/obstacle_entity.h"
#include "../entities/beat_map.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"
#include "../components/obstacle.h"
#include "../constants.h"
#include "../entities/settings.h"
#include "../util/lane_utils.h"

#include <cmath>

namespace {

// Per-kind schedule context shared across the 3 spawn transforms.
struct ScheduleContext {
    entt::registry& reg;
    SongState&      song;
    const BeatMap&  map;
    float           audio_offset_sec;
};

// Resolve the calibrated arrival time for a beat. Mirrors the prior
// single-loop logic; called once per entry. Returns true if the spawn
// should proceed; populates `effective_spawn_time`, `start_y`, and
// `calibrated_arrival_time` for the caller.
bool resolve_spawn_timing(const ScheduleContext& ctx,
                          const BeatEntry& entry,
                          float& calibrated_arrival_time,
                          float& effective_spawn_time,
                          float& start_y) {
    float beat_time = ctx.song.offset + entry.beat_index * ctx.song.beat_period;
    if (!entry.has_time_sec &&
        !ctx.map.beat_times.empty() &&
        entry.beat_index >= 0 &&
        static_cast<size_t>(entry.beat_index) < ctx.map.beat_times.size()) {
        beat_time = ctx.map.beat_times[static_cast<size_t>(entry.beat_index)];
    }
    if (entry.has_time_sec) {
        beat_time = entry.time_sec;
    }

    calibrated_arrival_time = beat_time + ctx.audio_offset_sec;
    float spawn_time = calibrated_arrival_time - ctx.song.lead_time;
    if (ctx.song.song_time < spawn_time) return false;

    float overshoot = ctx.song.song_time - spawn_time;
    start_y = constants::SPAWN_Y + overshoot * ctx.song.scroll_speed;
    const float max_start_y = constants::PLAYER_Y;
    effective_spawn_time = spawn_time;
    if (start_y > max_start_y) {
        start_y = max_start_y;
        effective_spawn_time = ctx.song.song_time
            - (max_start_y - constants::SPAWN_Y) / ctx.song.scroll_speed;
    }
    return true;
}

void warn_invalid_lane_once(int lane) {
    if (!lane_utils::is_valid(lane)) {
        TraceLog(LOG_WARNING, "Invalid beatmap lane %d; defaulting to center lane", lane);
    }
}

// Per-kind transforms: one while-loop per kind cursor. Replaces the
// former single switch-on-discriminator loop (issue #1202/#1204).

void schedule_shape_gates(ScheduleContext& ctx) {
    while (ctx.song.next_shape_gate_idx < ctx.map.shape_gate_beats.size()) {
        const auto& entry = ctx.map.shape_gate_beats[ctx.song.next_shape_gate_idx];
        float calibrated_arrival_time = 0.0f;
        float effective_spawn_time    = 0.0f;
        float start_y                 = 0.0f;
        if (!resolve_spawn_timing(ctx, entry, calibrated_arrival_time,
                                  effective_spawn_time, start_y)) break;

        const int8_t spawn_lane = lane_utils::valid_or_default(entry.lane);
        const float x_pos = constants::LANE_X[static_cast<int>(spawn_lane)];
        warn_invalid_lane_once(static_cast<int>(entry.lane));

        const BeatInfo bi{entry.beat_index, calibrated_arrival_time, effective_spawn_time};
        spawn_shape_gate_rhythm(ctx.reg, {x_pos, start_y, entry.shape,
                                          ctx.song.scroll_speed}, bi);
        ctx.song.next_shape_gate_idx++;
    }
}

void schedule_split_paths(ScheduleContext& ctx) {
    while (ctx.song.next_split_path_idx < ctx.map.split_path_beats.size()) {
        const auto& entry = ctx.map.split_path_beats[ctx.song.next_split_path_idx];
        float calibrated_arrival_time = 0.0f;
        float effective_spawn_time    = 0.0f;
        float start_y                 = 0.0f;
        if (!resolve_spawn_timing(ctx, entry, calibrated_arrival_time,
                                  effective_spawn_time, start_y)) break;

        const int8_t spawn_lane = lane_utils::valid_or_default(entry.lane);
        const float x_pos = constants::LANE_X[static_cast<int>(spawn_lane)];
        warn_invalid_lane_once(static_cast<int>(entry.lane));

        const BeatInfo bi{entry.beat_index, calibrated_arrival_time, effective_spawn_time};
        spawn_split_path_rhythm(ctx.reg, {x_pos, start_y, entry.shape,
                                          spawn_lane, ctx.song.scroll_speed}, bi);
        ctx.song.next_split_path_idx++;
    }
}

void schedule_onset_markers(ScheduleContext& ctx) {
    while (ctx.song.next_onset_marker_idx < ctx.map.onset_marker_beats.size()) {
        const auto& entry = ctx.map.onset_marker_beats[ctx.song.next_onset_marker_idx];
        float calibrated_arrival_time = 0.0f;
        float effective_spawn_time    = 0.0f;
        float start_y                 = 0.0f;
        if (!resolve_spawn_timing(ctx, entry, calibrated_arrival_time,
                                  effective_spawn_time, start_y)) break;

        const int8_t spawn_lane = lane_utils::valid_or_default(entry.lane);
        const float x_pos = constants::LANE_X[static_cast<int>(spawn_lane)];
        warn_invalid_lane_once(static_cast<int>(entry.lane));

        const BeatInfo bi{entry.beat_index, calibrated_arrival_time, effective_spawn_time};
        spawn_onset_marker_rhythm(ctx.reg, {x_pos, start_y, ctx.song.scroll_speed}, bi);
        ctx.song.next_onset_marker_idx++;
    }
}

} // namespace

void beat_scheduler_system(entt::registry& reg, [[maybe_unused]] float dt) {
    auto* song = reg.ctx().find<SongState>();
    auto* map  = find_beat_map(reg);
    if (!song || !map || !song->playing) return;
    if (!std::isfinite(song->scroll_speed) || song->scroll_speed <= 0.0f) {
        TraceLog(LOG_WARNING, "beat_scheduler_system skipped: invalid scroll_speed %.3f", song->scroll_speed);
        return;
    }

    // Player calibration (#474). Positive audio_offset_ms delays beat timing:
    // we push spawn_time forward by the same delta so obstacles arrive at
    // PLAYER_Y later, matching the player's perceived audio lag. Negative
    // values advance beat timing. See entities/settings.h.
    const auto* settings = find_settings_state(reg);
    const float audio_offset_sec = settings ? settings::audio_offset_seconds(*settings) : 0.0f;

    ScheduleContext ctx{reg, *song, *map, audio_offset_sec};
    schedule_shape_gates(ctx);
    schedule_split_paths(ctx);
    schedule_onset_markers(ctx);
}
