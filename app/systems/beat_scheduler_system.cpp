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

// Per-kind schedule context shared across the per-(kind, shape, timing-source)
// spawn transforms.
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
//
// `beat_time` is the bin-source-resolved arrival time: for indexed bins
// the caller passes `beat_times[beat_index]` (or the BPM-derived grid
// fallback when beat_times are absent); for timed bins it passes the
// authored `BeatEntryTimed::time_sec`. The `entry.has_time_sec` branch
// no longer exists (#1533 Site #4) — bin membership IS the timing source.
bool resolve_spawn_timing(const ScheduleContext& ctx,
                          float beat_time,
                          float& calibrated_arrival_time,
                          float& effective_spawn_time,
                          float& start_y) {
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

// Per-(kind, shape, timing-source) transforms: one while-loop per cursor.
// Replaces the former per-kind loops that switched on `entry.shape` to
// pick the spawn target (issue #1202/#1204). The shape is now encoded by
// which vector the cursor walks and the shape literal each transform
// passes to the spawn function; the timing source is now encoded by which
// of the indexed (`*_beats`) or authored (`*_beats_timed`) sibling vector
// the cursor walks (issue #1533 Site #4).
//
// The per-row body (timing resolve, lane validate, lane-x lookup, warn,
// spawn-call) is identical across `shape_gate`, `split_path`, and
// `onset_marker` bins (issue #1416); only the trailing spawn call
// differs. `schedule_bin` owns the shared body; each kind passes a thin
// spawn lambda that picks the right `spawn_*_rhythm` call. The bin/spawn
// templates accept any row type exposing `beat_index` + `lane`; the
// caller's `time_fn` projects the row to its arrival `beat_time`.

template <typename Row, typename TimeFn, typename SpawnFn>
void schedule_bin(ScheduleContext& ctx,
                  const std::vector<Row>& bin,
                  size_t& cursor,
                  TimeFn&& time_fn,
                  SpawnFn&& spawn_fn) {
    while (cursor < bin.size()) {
        const auto& entry = bin[cursor];
        float calibrated_arrival_time = 0.0f;
        float effective_spawn_time    = 0.0f;
        float start_y                 = 0.0f;
        const float beat_time = time_fn(entry);
        if (!resolve_spawn_timing(ctx, beat_time, calibrated_arrival_time,
                                  effective_spawn_time, start_y)) break;

        const int8_t spawn_lane = lane_utils::valid_or_default(entry.lane);
        const float x_pos = constants::LANE_X[static_cast<int>(spawn_lane)];
        if (!lane_utils::is_valid(static_cast<int>(entry.lane))) {
            TraceLog(LOG_WARNING, "Invalid beatmap lane %d; defaulting to center lane",
                     static_cast<int>(entry.lane));
        }

        const BeatInfo bi{entry.beat_index, calibrated_arrival_time, effective_spawn_time};
        spawn_fn(ctx, spawn_lane, x_pos, start_y, bi);
        cursor++;
    }
}

// Indexed bins resolve `beat_time` from `beat_times[beat_index]` when the
// onset table is loaded; otherwise they fall back to the BPM-derived grid
// time. Timed bins resolve `beat_time` directly from the authored
// `BeatEntryTimed::time_sec` column — membership in a `*_beats_timed`
// vector IS the precondition that an authored timestamp exists.
auto make_indexed_time_fn(const ScheduleContext& ctx) {
    return [&ctx](const BeatEntry& entry) {
        float beat_time = ctx.song.offset + entry.beat_index * ctx.song.beat_period;
        if (!ctx.map.beat_times.empty() &&
            entry.beat_index >= 0 &&
            static_cast<size_t>(entry.beat_index) < ctx.map.beat_times.size()) {
            beat_time = ctx.map.beat_times[static_cast<size_t>(entry.beat_index)];
        }
        return beat_time;
    };
}

auto make_timed_time_fn() {
    return [](const BeatEntryTimed& entry) { return entry.time_sec; };
}

template <typename Row, typename TimeFn>
void schedule_shape_gate_bin(ScheduleContext& ctx,
                             const std::vector<Row>& bin,
                             size_t& cursor,
                             TimeFn&& time_fn,
                             Shape required_shape) {
    schedule_bin(ctx, bin, cursor, std::forward<TimeFn>(time_fn),
        [required_shape](ScheduleContext& c, int8_t /*spawn_lane*/,
                         float x_pos, float start_y, const BeatInfo& bi) {
            spawn_shape_gate_rhythm(c.reg, {x_pos, start_y, required_shape,
                                            c.song.scroll_speed}, bi);
        });
}

template <typename Row, typename TimeFn>
void schedule_split_path_bin(ScheduleContext& ctx,
                             const std::vector<Row>& bin,
                             size_t& cursor,
                             TimeFn&& time_fn,
                             Shape required_shape) {
    schedule_bin(ctx, bin, cursor, std::forward<TimeFn>(time_fn),
        [required_shape](ScheduleContext& c, int8_t spawn_lane,
                         float x_pos, float start_y, const BeatInfo& bi) {
            spawn_split_path_rhythm(c.reg, {x_pos, start_y, required_shape,
                                            spawn_lane, c.song.scroll_speed}, bi);
        });
}

template <typename Row, typename TimeFn>
void schedule_onset_marker_bin(ScheduleContext& ctx,
                               const std::vector<Row>& bin,
                               size_t& cursor,
                               TimeFn&& time_fn) {
    schedule_bin(ctx, bin, cursor, std::forward<TimeFn>(time_fn),
        [](ScheduleContext& c, int8_t /*spawn_lane*/,
           float x_pos, float start_y, const BeatInfo& bi) {
            spawn_onset_marker_rhythm(c.reg, {x_pos, start_y,
                                              c.song.scroll_speed}, bi);
        });
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
    const auto indexed_time = make_indexed_time_fn(ctx);
    const auto timed_time   = make_timed_time_fn();

    schedule_shape_gate_bin(ctx, ctx.map.shape_gate_circle_beats,
                            ctx.song.next_shape_gate_circle_idx, indexed_time, Shape::Circle);
    schedule_shape_gate_bin(ctx, ctx.map.shape_gate_square_beats,
                            ctx.song.next_shape_gate_square_idx, indexed_time, Shape::Square);
    schedule_shape_gate_bin(ctx, ctx.map.shape_gate_triangle_beats,
                            ctx.song.next_shape_gate_triangle_idx, indexed_time, Shape::Triangle);
    schedule_split_path_bin(ctx, ctx.map.split_path_circle_beats,
                            ctx.song.next_split_path_circle_idx, indexed_time, Shape::Circle);
    schedule_split_path_bin(ctx, ctx.map.split_path_square_beats,
                            ctx.song.next_split_path_square_idx, indexed_time, Shape::Square);
    schedule_split_path_bin(ctx, ctx.map.split_path_triangle_beats,
                            ctx.song.next_split_path_triangle_idx, indexed_time, Shape::Triangle);
    schedule_onset_marker_bin(ctx, ctx.map.onset_marker_beats,
                              ctx.song.next_onset_marker_idx, indexed_time);

    schedule_shape_gate_bin(ctx, ctx.map.shape_gate_circle_beats_timed,
                            ctx.song.next_shape_gate_circle_timed_idx, timed_time, Shape::Circle);
    schedule_shape_gate_bin(ctx, ctx.map.shape_gate_square_beats_timed,
                            ctx.song.next_shape_gate_square_timed_idx, timed_time, Shape::Square);
    schedule_shape_gate_bin(ctx, ctx.map.shape_gate_triangle_beats_timed,
                            ctx.song.next_shape_gate_triangle_timed_idx, timed_time, Shape::Triangle);
    schedule_split_path_bin(ctx, ctx.map.split_path_circle_beats_timed,
                            ctx.song.next_split_path_circle_timed_idx, timed_time, Shape::Circle);
    schedule_split_path_bin(ctx, ctx.map.split_path_square_beats_timed,
                            ctx.song.next_split_path_square_timed_idx, timed_time, Shape::Square);
    schedule_split_path_bin(ctx, ctx.map.split_path_triangle_beats_timed,
                            ctx.song.next_split_path_triangle_timed_idx, timed_time, Shape::Triangle);
    schedule_onset_marker_bin(ctx, ctx.map.onset_marker_beats_timed,
                              ctx.song.next_onset_marker_timed_idx, timed_time);
}
