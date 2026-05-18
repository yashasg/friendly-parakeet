#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../util/rhythm_math.h"
#include "../util/shape_tag.h"
#include "../components/song_state.h"
#include "gameplay_intents.h"
#include "../constants.h"
#include "../util/lane_utils.h"
#include <cmath>

namespace {

template <typename RequiredTag, typename ShapeTag, typename TargetTag>
bool shape_row_match(entt::registry& reg,
                     entt::entity player_entity,
                     entt::entity obstacle_entity,
                     bool rhythm_mode) {
    if (!reg.all_of<RequiredTag>(obstacle_entity)) return false;
    if (rhythm_mode) {
        if (reg.all_of<ShapeWindowMorphInTag>(player_entity)) {
            // Pressed presence == "the window has a press" (Fabian Principle 3,
            // issue #1533) — replaces the former `sw.press_time >= 0.0f`
            // sentinel comparison.
            return reg.all_of<Pressed>(player_entity)
                && reg.all_of<TargetTag>(player_entity);
        }
        if (reg.all_of<ShapeWindowActiveTag>(player_entity)) {
            return reg.any_of<ShapeTag, TargetTag>(player_entity);
        }
        // ShapeWindowMorphOutTag and Idle both reject in rhythm mode.
        return false;
    }
    if (reg.all_of<ShapeTag>(player_entity)) return true;
    const bool window_open = reg.any_of<ShapeWindowMorphInTag,
                                        ShapeWindowActiveTag,
                                        ShapeWindowMorphOutTag>(player_entity);
    return window_open && reg.all_of<TargetTag>(player_entity);
}

bool player_matches_required_shape(entt::registry& reg,
                                    entt::entity player_entity,
                                    entt::entity obstacle_entity,
                                    bool rhythm_mode) {
    if (reg.all_of<RequiredShapeHexagonTag>(obstacle_entity)) {
        // No Hexagon-required obstacles are spawned in production; preserve
        // the legacy "Hexagon-required → unclearable" contract by short-
        // circuiting here regardless of player state.
        return false;
    }

    // Per-shape match check: each row of the shape table is its own
    // per-shape table-join (issue #1202/#1204), so we probe the three
    // playable rows by tag-presence. No `switch`, no `if (x == Shape::Bar)`
    // — each `shape_row_match` instantiation IS the per-row transform.
    if (shape_row_match<RequiredShapeCircleTag,   ShapeCircleTag,   TargetShapeCircleTag>  (reg, player_entity, obstacle_entity, rhythm_mode)) return true;
    if (shape_row_match<RequiredShapeSquareTag,   ShapeSquareTag,   TargetShapeSquareTag>  (reg, player_entity, obstacle_entity, rhythm_mode)) return true;
    if (shape_row_match<RequiredShapeTriangleTag, ShapeTriangleTag, TargetShapeTriangleTag>(reg, player_entity, obstacle_entity, rhythm_mode)) return true;
    return false;
}

bool shape_gate_lane_match(int8_t obstacle_lane, int player_lane) {
    return static_cast<int>(obstacle_lane) == player_lane;
}

}  // namespace

void collision_system(entt::registry& reg, [[maybe_unused]] float dt) {
    auto player_view = reg.view<PlayerTag, WorldPosition, PlayerShape, ShapeWindow, Lane>();
    if (player_view.begin() == player_view.end()) return;

    auto player_entity = *player_view.begin();
    auto [p_transform, p_shape, p_window, p_lane] =
        player_view.get<WorldPosition, PlayerShape, ShapeWindow, Lane>(player_entity);
    lane_utils::normalize(reg, player_entity, p_lane, &p_transform);
    (void)p_shape;

    auto& song = reg.ctx().get<SongState>();
    const bool rhythm_mode = song.playing || song.finished;

    // Frame-constant precomputes — both values are invariant across all obstacle
    // loops since player transform and vertical state don't change mid-frame.
    // Precomputing here avoids redundant addition + Vector2 construction per obstacle.
    // Only Jumping carries a y_offset; Sliding and Grounded leave it at 0.
    const auto* p_jump = reg.try_get<Jumping>(player_entity);
    const float player_y_offset = p_jump ? p_jump->y_offset : 0.0f;
    const float player_timing_y = p_transform.position.y + player_y_offset;
    const int player_lane       = lane_utils::nearest_lane_for_x(p_transform.position.x);

    // resolve: tag entity as scored (cleared) or missed.
    // kind is passed by the caller — no try_get needed since each per-kind
    // loop already holds the Obstacle component.
    auto resolve = [&](entt::entity entity, float obs_z, bool cleared) {
        if (obs_z < player_timing_y) return;

        if (!cleared) {
            // MISS — tag only; scoring_system owns energy drain and death-cause attribution.
            reg.get_or_emplace<MissTag>(entity);
            reg.get_or_emplace<ScoredTag>(entity);
            return;
        }

        reg.get_or_emplace<ScoredTag>(entity);
    };

    const auto* pressed = reg.try_get<Pressed>(player_entity);
    const bool can_grade_shape =
        song.half_window > 0.0f && pressed != nullptr;
    auto grade_shape_timing = [&](entt::entity entity, float arrival_time) {
        // can_grade_shape gates entry, so `pressed` is non-null here.
        float delta_seconds = std::abs(pressed->press_time - arrival_time);
        float precision = 1.0f - (delta_seconds / kTimingOkSeconds);
        if (precision < 0.0f) precision = 0.0f;
        if (precision > 1.0f) precision = 1.0f;
        reg.emplace_or_replace<TimingGrade>(entity, TimingGrade{precision});
        emplace_timing_tier_tag_for_delta_abs(reg, entity, delta_seconds);

        // WindowGraded presence == "this press has been graded" (Fabian
        // Principle 3, issue #1533) — replaces the former `p_window.graded`
        // bool.
        if (!reg.all_of<WindowGraded>(player_entity)) {
            float scale = window_scale_from_delta_abs(delta_seconds);
            const float active_elapsed = song.song_time - p_window.window_start;
            float remaining = song.window_duration - active_elapsed;
            if (remaining > 0.0f && scale < 1.0f) {
                // Adjust window_start backward so the song-time-derived
                // elapsed time is naturally larger on subsequent ticks,
                // causing the Active phase to expire earlier.  Mutating
                // window_timer directly would be overwritten by
                // shape_window_system on the next frame.
                p_window.window_start -= remaining * (1.0f - scale);
            }
            reg.emplace_or_replace<WindowGraded>(player_entity);
        }
    };

    auto resolve_shape_obstacle = [&](entt::entity entity,
                                      const WorldPosition& wt,
                                      bool lane_ok,
                                      const BeatInfo* timing_info) {
        if (!lane_ok) {
            resolve(entity, wt.position.y, false);
            return;
        }

        const bool shape_ok =
            player_matches_required_shape(reg, player_entity, entity, rhythm_mode);
        if (shape_ok && timing_info && can_grade_shape) {
            grade_shape_timing(entity, timing_info->arrival_time);
        }
        resolve(entity, wt.position.y, shape_ok);
    };

    // Per-kind structural views — each loop touches only entities that actually
    // carry the required components, eliminating per-entity try_get branches.
    // The required-shape data lives as a per-shape tag (issue #1202/#1204),
    // so the views below filter on `ShapeGateTag` / `SplitPathTag` and the
    // resolver dispatches per-shape via `player_matches_required_shape`.

    // ShapeGate: ShapeGateTag (carries RequiredShape*Tag + raw int8_t lane).
    // The raw int8_t component is the ShapeGate positional lane per the slot
    // reservation in `app/components/obstacle.h` (issue #1198). SplitPath
    // entities are filtered out by absence of `ShapeGateTag`.
    {
        auto rhythm_view = reg.view<ObstacleTag, Obstacle, WorldPosition, ShapeGateTag, int8_t, BeatInfo>(
            entt::exclude<ScoredTag, ResolvedObstacleTag>);
        for (auto [e, obstacle, wt, lane, info] : rhythm_view.each()) {
            (void)obstacle;
            const bool lane_ok = shape_gate_lane_match(lane, player_lane);
            resolve_shape_obstacle(e, wt, lane_ok, &info);
        }

        auto view = reg.view<ObstacleTag, Obstacle, WorldPosition, ShapeGateTag, int8_t>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, BeatInfo>);
        for (auto [e, obstacle, wt, lane] : view.each()) {
            (void)obstacle;
            const bool lane_ok = shape_gate_lane_match(lane, player_lane);
            resolve_shape_obstacle(e, wt, lane_ok, nullptr);
        }

        auto fallback_rhythm_view = reg.view<ObstacleTag, Obstacle, WorldPosition, ShapeGateTag, BeatInfo>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, int8_t>);
        for (auto [e, obstacle, wt, info] : fallback_rhythm_view.each()) {
            (void)obstacle;
            const bool lane_ok = lane_utils::nearest_lane_for_x(wt.position.x) == player_lane;
            resolve_shape_obstacle(e, wt, lane_ok, &info);
        }

        auto fallback_view = reg.view<ObstacleTag, Obstacle, WorldPosition, ShapeGateTag>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, BeatInfo, int8_t>);
        for (auto [e, obstacle, wt] : fallback_view.each()) {
            (void)obstacle;
            const bool lane_ok = lane_utils::nearest_lane_for_x(wt.position.x) == player_lane;
            resolve_shape_obstacle(e, wt, lane_ok, nullptr);
        }
    }

    // SplitPath: SplitPathTag (carries RequiredShape*Tag + raw int8_t lane).
    // The raw int8_t component is the required dodge lane per the slot
    // reservation in `app/components/obstacle.h` (issue #1198). ShapeGate
    // entities are filtered out by absence of `SplitPathTag`.
    {
        auto rhythm_view = reg.view<ObstacleTag, Obstacle, WorldPosition, SplitPathTag, int8_t, BeatInfo>(
            entt::exclude<ScoredTag, ResolvedObstacleTag>);
        for (auto [e, obstacle, wt, rlane, info] : rhythm_view.each()) {
            (void)obstacle;
            const bool lane_ok = player_lane == rlane;
            resolve_shape_obstacle(e, wt, lane_ok, &info);
        }

        auto view = reg.view<ObstacleTag, Obstacle, WorldPosition, SplitPathTag, int8_t>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, BeatInfo>);
        for (auto [e, obstacle, wt, rlane] : view.each()) {
            (void)obstacle;
            const bool lane_ok = player_lane == rlane;
            resolve_shape_obstacle(e, wt, lane_ok, nullptr);
        }
    }

}
