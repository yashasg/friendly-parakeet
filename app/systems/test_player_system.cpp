#include "all_systems.h"
#include "test_player.h"
#include "../components/game_state.h"
#include "input_events.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/obstacle.h"
#include "../components/rhythm.h"
#include "../components/scoring.h"
#include "session_logger_system.h"
#include "../util/lane_utils.h"
#include "../util/shape_lane_mapping.h"
#include "../util/shape_tag.h"
#include "../constants.h"

#include <raylib.h>
#include <array>
#include <cmath>
#include <random>
#include <string_view>

// Keep pro presses slightly ahead of beat arrival so the press is guaranteed
// to land before collision resolution in fixed-step ordering.
static constexpr float kProPressLeadSeconds = 0.033f;

// ── Helpers ──────────────────────────────────────────────────

static Rectangle lane_overlap_rect(float x) {
    return {x - constants::PLAYER_SIZE * 0.5f, 0.0f, constants::PLAYER_SIZE, 1.0f};
}

static bool lane_centers_overlap(float lhs_x, float rhs_x) {
    return CheckCollisionRecs(lane_overlap_rect(lhs_x), lane_overlap_rect(rhs_x));
}

// ── Per-shape press dispatch table (issue #1202 PR C3) ─────────
// One row per shape that the test player can actually press; identity
// is encoded by `&kShapePressCircle` / `&kShapePressSquare` /
// `&kShapePressTriangle`. `TestPlayerAction::shape_press == nullptr`
// means "no shape required" (replaces the former
// `target_shape == Shape::Hexagon` sentinel compare). Per Fabian's
// existential-processing canon, the pointer's value IS the choice —
// consumers never branch on a Shape value to dispatch.
struct ShapePressSpec {
    void           (*enqueue)(entt::dispatcher&);
    const char*      key_name;   // EXECUTE log: e.g. "key_1(Circle)"
    std::string_view label;      // PERCEIVE/PLAN log: e.g. "Circle"
};

namespace {
inline void enqueue_press_circle  (entt::dispatcher& d) { d.enqueue<ShapePressCircleEvent>({});   }
inline void enqueue_press_square  (entt::dispatcher& d) { d.enqueue<ShapePressSquareEvent>({});   }
inline void enqueue_press_triangle(entt::dispatcher& d) { d.enqueue<ShapePressTriangleEvent>({}); }

inline constexpr ShapePressSpec kShapePressCircle  {&enqueue_press_circle,   "key_1(Circle)",   "Circle"};
inline constexpr ShapePressSpec kShapePressSquare  {&enqueue_press_square,   "key_2(Square)",   "Square"};
inline constexpr ShapePressSpec kShapePressTriangle{&enqueue_press_triangle, "key_3(Triangle)", "Triangle"};

// Index-aligned with `shape_index(Shape)`. Hexagon's slot is `nullptr`
// because Hexagon is never a required-shape value — `current_required_shape`
// only returns Hexagon as the no-tag sentinel, which `determine_action`
// gates out via `has_required_shape_tag` before indexing this table.
inline constexpr std::array<const ShapePressSpec*, kShapeCount> kShapePressByIndex{
    &kShapePressCircle,
    &kShapePressSquare,
    &kShapePressTriangle,
    nullptr,
};
}  // namespace

static bool test_player_shape_done(const TestPlayerAction& action) {
    return action.shape_done;
}

static bool test_player_lane_done(const TestPlayerAction& action) {
    return action.lane_done;
}

static bool test_player_vertical_done(const TestPlayerAction& action) {
    return action.vertical_done;
}

static bool test_player_needs_lane(const TestPlayerAction& action) {
    return lane_utils::is_valid(action.target_lane) && !test_player_lane_done(action);
}

// Determine the required action for an obstacle.
static TestPlayerAction determine_action(
    entt::registry& reg, entt::entity entity,
    int8_t player_lane, const SongState& song)
{
    TestPlayerAction action;
    action.obstacle = entity;

    // Get arrival time from BeatInfo
    auto* beat = reg.try_get<BeatInfo>(entity);
    if (beat) {
        action.arrival_time = beat->arrival_time;
    } else {
        // Estimate from position + velocity
        auto* wt  = reg.try_get<WorldPosition>(entity);
        auto* vel = reg.try_get<Vector2>(entity);
        if (wt && vel && vel->y > 0.0f) {
            action.arrival_time = song.song_time +
                (constants::PLAYER_Y - wt->position.y) / vel->y;
        }
    }

    // Shape requirement
    if (has_required_shape_tag(reg, entity)) {
        const int shape_idx = shape_index(current_required_shape(reg, entity));
        action.shape_press = (shape_idx >= 0 && shape_idx < kShapeCount)
            ? kShapePressByIndex[static_cast<size_t>(shape_idx)]
            : nullptr;

        // ShapeGate: player must also be in the lane where the shape hole is.
        // The hole is at obs_pos.x — find which lane that corresponds to.
        auto* obs_wt = reg.try_get<WorldPosition>(entity);
        if (obs_wt && !reg.all_of<SplitPathTag>(entity)) {
            for (int i = 0; i < constants::LANE_COUNT; ++i) {
                if (!lane_centers_overlap(obs_wt->position.x, constants::LANE_X[i])) continue;
                if (i != player_lane) {
                    action.target_lane = static_cast<int8_t>(i);
                }
                break;
            }
        }
    }

    // Lane requirement (SplitPath: exact lane stored as raw int8_t per
    // issue #1198 / the slot-reservation note in app/components/obstacle.h).
    if (reg.all_of<SplitPathTag>(entity)) {
        if (auto* req_lane = reg.try_get<int8_t>(entity);
            req_lane && lane_utils::is_valid(*req_lane) && *req_lane != player_lane) {
            action.target_lane = *req_lane;
        }
    }

    return action;
}

// ── System ───────────────────────────────────────────────────

void test_player_system(entt::registry& reg, float dt) {
    auto* state = reg.ctx().find<TestPlayerState>();
    if (!state || !state->active) return;

    auto& gs    = reg.ctx().get<GameState>();
    auto& ctx   = reg.ctx();
    auto& disp  = ctx.get<entt::dispatcher>();
    auto* log   = ctx.find<SessionLog>();
    auto* song  = ctx.find<SongState>();
    float song_time = song ? song->song_time : 0.0f;

    // Per Fabian's existential processing (issue #1202/#1204, PR F): each
    // former `gs.phase == GamePhase::X` branch dispatches on the per-phase
    // ctx-tag mirror seeded by `enter_phase<...>()`.
    // `phase_timer` stays a scalar on `GameState` until PR G deletes the
    // enum-typed field.
    const bool title_phase         = ctx.contains<GamePhaseTitleTag>();
    const bool game_over_phase     = ctx.contains<GamePhaseGameOverTag>();
    const bool song_complete_phase = ctx.contains<GamePhaseSongCompleteTag>();
    const bool paused_phase        = ctx.contains<GamePhasePausedTag>();
    const bool level_select_phase  = ctx.contains<GamePhaseLevelSelectTag>();
    const bool playing_phase       = ctx.contains<GamePhasePlayingTag>();

    // ── AUTO-START ───────────────────────────────────────────
    if (title_phase || game_over_phase || song_complete_phase) {
        if (gs.phase_timer > constants::TEST_PLAYER_AUTO_START_DELAY) {
            // On end screens, press Restart; on Title, press Confirm.
            // Per #1277: each action is its own event type — no enum.
            if (game_over_phase || song_complete_phase) {
                disp.enqueue<MenuRestartEvent>({});
            } else {
                disp.enqueue<MenuConfirmEvent>({});
            }
            if (log) {
                const char* phase_name =
                    title_phase         ? "Title" :
                    song_complete_phase ? "SongComplete" : "GameOver";
                session_log_write(*log, song_time, "PLAYER",
                    "AUTO_START phase=%s", phase_name);

                // Log song results before replaying
                if (song_complete_phase) {
                    auto* results = ctx.find<SongResults>();
                    auto* score = ctx.find<ScoreState>();
                    if (results && score) {
                        session_log_write(*log, song_time, "GAME",
                            "SONG_END result=CLEAR score=%d perfect=%d good=%d ok=%d bad=%d miss=%d",
                            score->score,
                            results->perfect_count, results->good_count,
                            results->ok_count, results->bad_count,
                            results->miss_count);
                    }
                }
            }
        }
        return;
    }

    if (paused_phase) {
        disp.enqueue<MenuConfirmEvent>({});
        return;
    }

    // Auto-confirm on LevelSelect (level/difficulty already set in main.cpp)
    if (level_select_phase && gs.phase_timer > constants::UI_ENTRY_DEBOUNCE) {
        disp.enqueue<MenuConfirmEvent>({});
        return;
    }

    if (!playing_phase) return;
    if (!song) return;

    // Reset stale state when a new play session starts (after enter_playing
    // calls reg.clear(), all old entity IDs are invalid).
    if (song->song_time < 0.01f && state->action_count > 0) {
        state->action_count = 0;
    }

    const auto& cfg = state->skill;

    // ── Find player ──────────────────────────────────────────
    auto player_view = reg.view<PlayerTag, WorldPosition, PlayerShape, ShapeWindow, Lane>();
    if (player_view.begin() == player_view.end()) return;

    auto player_entity = *player_view.begin();
    auto [p_transform, p_shape, p_window, p_lane] =
        player_view.get<WorldPosition, PlayerShape, ShapeWindow, Lane>(player_entity);
    lane_utils::normalize(reg, player_entity, p_lane, &p_transform);

    // Per-frame constant: player's vertical offset (Grounded/Sliding → 0,
    // Jumping → parabolic arc) used in collision-zone math below.
    const auto* p_jump = reg.try_get<Jumping>(player_entity);
    const float player_y_offset = p_jump ? p_jump->y_offset : 0.0f;

    // ── PERCEIVE: scan obstacles in vision range ─────────────
    // Compute the "effective lane" — where the player will be after
    // all pending lane changes in the action queue execute. A
    // `LaneTransition` row on the player IS "lane change in flight"
    // (Fabian Principle 3, issue #1533); read its target lane directly.
    int8_t effective_lane = p_lane.current;
    if (const auto* p_transition = reg.try_get<LaneTransition>(player_entity);
        p_transition && lane_utils::is_valid(p_transition->target)) {
        effective_lane = p_transition->target;
    }
    for (int i = 0; i < state->action_count; ++i) {
        if (lane_utils::is_valid(state->actions[i].target_lane) &&
            !test_player_lane_done(state->actions[i])) {
            effective_lane = state->actions[i].target_lane;
        }
    }

    auto obs_view = reg.view<ObstacleTag, WorldPosition, Obstacle>(
        entt::exclude<ScoredTag, TestPlayerPlannedTag, NonScorableTag>);
    for (auto [entity, obs_wt, obs] : obs_view.each()) {
        (void)obs;
        float dist = p_transform.position.y - obs_wt.position.y;
        if (dist <= 0.0f || dist > cfg.vision_range) continue;

        TestPlayerAction action = determine_action(reg, entity, effective_lane, *song);

        // Roll reaction timer
        std::uniform_real_distribution<float> reaction_dist(cfg.reaction_min, cfg.reaction_max);
        float reaction = reaction_dist(state->rng);

        // Pro player aims for Perfect timing on SHAPE PRESSES.
        // Lane dodges and vertical-only actions react ASAP — no delay.
        bool has_shape = (action.shape_press != nullptr);
        bool has_lane_or_vertical = (lane_utils::is_valid(action.target_lane) ||
                                     action.wants_jump || action.wants_slide);

        if (cfg.aim_perfect && has_shape) {
            float ideal_press = action.arrival_time - kProPressLeadSeconds;
            action.shape_not_before_time = ideal_press;
            float time_until_ideal = ideal_press - song->song_time;
            if (!has_lane_or_vertical && time_until_ideal > cfg.reaction_min) {
                action.timer = time_until_ideal;
                if (log) {
                    session_log_write(*log, song_time, "PLAYER",
                        "WAIT delaying %.3fs (aiming for Perfect shape press)", time_until_ideal);
                }
            } else {
                action.timer = reaction;
            }
        } else {
            action.timer = reaction;
        }

        if (state->action_count < TestPlayerState::MAX_ACTIONS) {
            state->actions[state->action_count++] = action;
        }
        reg.get_or_emplace<TestPlayerPlannedTag>(entity);

        if (log) {
            auto* beat_info = reg.try_get<BeatInfo>(entity);
            int beat_num = beat_info ? beat_info->beat_index : -1;
            const std::string_view kind_name = obstacle_kind_label(reg, entity);
            // Preserve the legacy PERCEIVE log format: when no shape is
            // required, the field reads "Hexagon" (the former sentinel).
            const std::string_view shape_name =
                action.shape_press ? action.shape_press->label : std::string_view{"Hexagon"};

            session_log_write(*log, song_time, "PLAYER",
                "PERCEIVE obstacle=%u beat=%d kind=%.*s shape=%.*s lane=%d dist=%.0fpx",
                static_cast<unsigned>(entt::to_integral(entity)),
                beat_num,
                static_cast<int>(kind_name.size()), kind_name.data(),
                static_cast<int>(shape_name.size()), shape_name.data(),
                action.target_lane, dist);

            const std::string_view action_shape_name =
                action.shape_press ? action.shape_press->label : std::string_view{""};
            session_log_write(*log, song_time, "PLAYER",
                "PLAN action=%.*s%s%s react=%.3fs arrival=%.3fs",
                static_cast<int>(action_shape_name.size()), action_shape_name.data(),
                lane_utils::is_valid(action.target_lane) ? "+lane" : "",
                action.wants_jump  ? "+jump"  :
                action.wants_slide ? "+slide" : "",
                action.timer, action.arrival_time);
        }
    }

    // ── TICK timers ──────────────────────────────────────────
    for (int i = 0; i < state->action_count; ++i) {
        state->actions[i].timer -= dt;
    }
    if (state->swipe_cooldown_timer > 0.0f) {
        state->swipe_cooldown_timer -= dt;
    }

    // ── EXECUTE ready actions ────────────────────────────────
    // Only ONE key injection per frame.
    // Don't execute lane/vertical changes while a CLOSER unscored obstacle
    // is still approaching or passing through. A human player would see
    // "there's something coming first" and wait for it to resolve.
    // Shape presses are fine — they're how you clear shape gates.
    // Zone blocking and shape-pending guards are computed per-action below
    // to avoid self-blocking (an obstacle can't block its own dodge).
    // Track which obstacle has a pending shape press (pressed but not yet scored).
    // Lane changes for OTHER obstacles should wait, but lane changes for the
    // SAME obstacle are fine — they're part of clearing it.
    entt::entity pending_shape_obstacle = entt::null;
    for (int i = 0; i < state->action_count; ++i) {
        auto& a = state->actions[i];
        // Shape press fired but obstacle hasn't been scored yet.
        // scoring_system removes Obstacle component after processing.
        if (a.shape_press != nullptr && test_player_shape_done(a)
            && reg.valid(a.obstacle) && reg.all_of<Obstacle>(a.obstacle)
            && !reg.all_of<ScoredTag>(a.obstacle)) {
            pending_shape_obstacle = a.obstacle;
            break;
        }
    }

    bool key_injected = false;

    // Process actions in arrival order (closest obstacle first)
    // so we don't skip a nearer obstacle to act on a farther one.
    int exec_order[TestPlayerState::MAX_ACTIONS];
    for (int i = 0; i < state->action_count; ++i) exec_order[i] = i;
    for (int i = 0; i < state->action_count - 1; ++i) {
        for (int j = i + 1; j < state->action_count; ++j) {
            if (state->actions[exec_order[j]].arrival_time <
                state->actions[exec_order[i]].arrival_time) {
                int tmp = exec_order[i];
                exec_order[i] = exec_order[j];
                exec_order[j] = tmp;
            }
        }
    }

    for (int ei = 0; ei < state->action_count && !key_injected; ++ei) {
        auto& action = state->actions[exec_order[ei]];
        if (action.timer > 0.0f) continue;

        // Get beat number for logging
        int act_beat = -1;
        if (reg.valid(action.obstacle)) {
            auto* bi = reg.try_get<BeatInfo>(action.obstacle);
            if (bi) act_beat = bi->beat_index;
        }

        // Priority 1: Shape change
        bool too_early_for_shape = (cfg.aim_perfect &&
                                    action.shape_not_before_time > 0.0f &&
                                    song->song_time < action.shape_not_before_time);
        bool waiting_on_lane = test_player_needs_lane(action);
        if (action.shape_press != nullptr && !test_player_shape_done(action) && !waiting_on_lane && !too_early_for_shape) {
            action.shape_press->enqueue(disp);
            action.shape_done = true;
            key_injected = true;

            if (log) {
                session_log_write(*log, song_time, "PLAYER",
                    "EXECUTE %s for obstacle=%u beat=%d",
                    action.shape_press->key_name,
                    static_cast<unsigned>(entt::to_integral(action.obstacle)),
                    act_beat);
            }
            continue;
        }

        // Priority 2: Lane change
        // Wait for any obstacle to fully pass through collision zone before moving.
        // A human would see the obstacle passing and wait for it to clear.
        // Block lane change if a DIFFERENT obstacle's shape press is unresolved,
        // or if a DIFFERENT obstacle is in the collision zone.
        bool blocked_by_shape = (pending_shape_obstacle != entt::null
                                 && pending_shape_obstacle != action.obstacle);
        bool zone_blocked = false;
        {
            auto zone_view = reg.view<ObstacleTag, Obstacle, WorldPosition>(
                entt::exclude<ScoredTag, NonScorableTag>);
            for (auto [ze, obstacle, zwt] : zone_view.each()) {
                (void)obstacle;
                if (ze == action.obstacle) continue; // don't self-block
                float zdist = p_transform.position.y - zwt.position.y + player_y_offset;
                if (zdist >= -constants::COLLISION_MARGIN && zdist <= constants::COLLISION_MARGIN * 3.0f) {
                    zone_blocked = true;
                    break;
                }
            }
        }

        // Would moving to target_lane cause a MISS on any CLOSER unresolved
        // obstacle? A human player would see "something is in the way" and
        // wait. Don't move for a far obstacle if it breaks a closer one.
        bool move_would_fail_closer = false;
        if (test_player_needs_lane(action)) {
            int8_t next_lane = p_lane.current;
            if (action.target_lane < next_lane) next_lane--;
            else if (action.target_lane > next_lane) next_lane++;

            auto closer_view = reg.view<ObstacleTag, Obstacle, WorldPosition>(
                entt::exclude<ScoredTag, NonScorableTag>);
            for (auto [oe, obstacle, owt] : closer_view.each()) {
                (void)obstacle;
                if (oe == action.obstacle) continue;
                float odist = p_transform.position.y - owt.position.y + player_y_offset;
                if (odist <= 0.0f) continue;

                auto* obeat = reg.try_get<BeatInfo>(oe);
                if (obeat) {
                    if (obeat->arrival_time >= action.arrival_time) continue; // farther, don't care
                } else if (std::isfinite(song->scroll_speed) && song->scroll_speed > 0.0f) {
                    const float o_arrival = song_time + odist / song->scroll_speed;
                    if (o_arrival >= action.arrival_time) continue; // farther, don't care
                }

                // Closer obstacle — check if proposed lane is safe for it
                auto* oblocked = reg.try_get<uint8_t>(oe);
                if (oblocked && ((*oblocked >> next_lane) & 1)) {
                    move_would_fail_closer = true;
                    break;
                }
                if (has_required_shape_tag(reg, oe)) {
                    float lane_x = constants::LANE_X[next_lane];
                    if (!lane_centers_overlap(owt.position.x, lane_x)) {
                        move_would_fail_closer = true;
                        break;
                    }
                }
            }
        }

        if (test_player_needs_lane(action) && !reg.all_of<LaneTransition>(player_entity) &&
            state->swipe_cooldown_timer <= 0.0f
            && !zone_blocked && !blocked_by_shape && !move_would_fail_closer) {
            if (action.target_lane < p_lane.current) {
                disp.enqueue<GoLeftEvent>({});
                if (log) {
                    session_log_write(*log, song_time, "PLAYER",
                        "EXECUTE Go(Left) for obstacle=%u beat=%d",
                        static_cast<unsigned>(entt::to_integral(action.obstacle)), act_beat);
                }
            } else if (action.target_lane > p_lane.current) {
                disp.enqueue<GoRightEvent>({});
                if (log) {
                    session_log_write(*log, song_time, "PLAYER",
                        "EXECUTE Go(Right) for obstacle=%u beat=%d",
                        static_cast<unsigned>(entt::to_integral(action.obstacle)), act_beat);
                }
            }
            // Start cooldown so multi-lane moves have realistic delay
            state->swipe_cooldown_timer = TestPlayerState::SWIPE_COOLDOWN;
            // Check if we've reached the target
            if (p_lane.current == action.target_lane) {
                action.lane_done = true;
            }
            key_injected = true;
            continue;
        }

        // Priority 3: Vertical action (wait for other obstacles to clear zone)
        bool vert_blocked_by_shape = (pending_shape_obstacle != entt::null
                                      && pending_shape_obstacle != action.obstacle);
        bool vert_zone_blocked = false;
        {
            auto zone_view = reg.view<ObstacleTag, Obstacle, WorldPosition>(
                entt::exclude<ScoredTag, NonScorableTag>);
            for (auto [ze, obstacle, zwt] : zone_view.each()) {
                (void)obstacle;
                if (ze == action.obstacle) continue;
                float zdist = p_transform.position.y - zwt.position.y + player_y_offset;
                if (zdist >= -constants::COLLISION_MARGIN && zdist <= constants::COLLISION_MARGIN * 3.0f) {
                    vert_zone_blocked = true;
                    break;
                }
            }
        }
        if ((action.wants_jump || action.wants_slide) && !test_player_vertical_done(action) && !reg.any_of<Jumping, Sliding>(player_entity)
            && !vert_zone_blocked && !vert_blocked_by_shape) {
            if (action.wants_jump) {
                disp.enqueue<GoUpEvent>({});
                if (log) {
                    session_log_write(*log, song_time, "PLAYER",
                        "EXECUTE Go(Up) for obstacle=%u beat=%d",
                        static_cast<unsigned>(entt::to_integral(action.obstacle)), act_beat);
                }
            } else if (action.wants_slide) {
                disp.enqueue<GoDownEvent>({});
                if (log) {
                    session_log_write(*log, song_time, "PLAYER",
                        "EXECUTE Go(Down) for obstacle=%u beat=%d",
                        static_cast<unsigned>(entt::to_integral(action.obstacle)), act_beat);
                }
            }
            action.vertical_done = true;
            key_injected = true;
            continue;
        }
    }

    // ── CLEANUP completed actions ────────────────────────────
    for (int i = state->action_count - 1; i >= 0; --i) {
        auto& action = state->actions[i];
        const bool shape_done = (action.shape_press == nullptr) ||
                                test_player_shape_done(action);
        const bool lane_done = !lane_utils::is_valid(action.target_lane) ||
                               test_player_lane_done(action);
        const bool vertical_done = !(action.wants_jump || action.wants_slide) ||
                                   test_player_vertical_done(action);
        const bool done = shape_done && lane_done && vertical_done;
        // Entity no longer an active obstacle (scored + processed by scoring_system,
        // or destroyed by obstacle_despawn_system)
        bool expired = !reg.valid(action.obstacle) ||
                       !reg.all_of<Obstacle>(action.obstacle);
        if (done || expired) {
            state->actions[i] = state->actions[state->action_count - 1];
            --state->action_count;
        }
    }
}
