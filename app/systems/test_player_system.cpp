#include "all_systems.h"
#include "../components/test_player.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/rhythm.h"
#include "../components/scoring.h"
#include "../session_logger.h"
#include "../enum_names.h"
#include "../platform.h"
#include "../constants.h"

#include <cmath>
#include <random>

// ── Helpers ──────────────────────────────────────────────────

static const char* shape_key_name(Shape s) {
    switch (s) {
        case Shape::Circle:   return "key_1(Circle)";
        case Shape::Square:   return "key_2(Square)";
        case Shape::Triangle: return "key_3(Triangle)";
        default:              return "key_?(?)";
    }
}

// Find nearest unblocked lane to current position.
static int8_t nearest_unblocked_lane(uint8_t blocked_mask, int8_t current) {
    // Try current lane first
    if (!((blocked_mask >> current) & 1)) return current;

    // Expand outward from current
    for (int dist = 1; dist < constants::LANE_COUNT; ++dist) {
        int8_t left  = current - static_cast<int8_t>(dist);
        int8_t right = current + static_cast<int8_t>(dist);
        if (left >= 0 && !((blocked_mask >> left) & 1))  return left;
        if (right < constants::LANE_COUNT && !((blocked_mask >> right) & 1)) return right;
    }
    return current; // all blocked — shouldn't happen in valid beatmaps
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
        auto* pos = reg.try_get<Position>(entity);
        auto* vel = reg.try_get<Velocity>(entity);
        if (pos && vel && vel->dy > 0.0f) {
            action.arrival_time = song.song_time +
                (constants::PLAYER_Y - pos->y) / vel->dy;
        }
    }

    // Shape requirement
    auto* req_shape = reg.try_get<RequiredShape>(entity);
    if (req_shape) {
        action.target_shape = req_shape->shape;

        // ShapeGate: player must also be in the lane where the shape hole is.
        // The hole is at obs_pos.x — find which lane that corresponds to.
        auto* obs_pos = reg.try_get<Position>(entity);
        if (obs_pos && !reg.all_of<BlockedLanes>(entity) && !reg.all_of<RequiredLane>(entity)) {
            for (int i = 0; i < constants::LANE_COUNT; ++i) {
                if (std::abs(obs_pos->x - constants::LANE_X[i]) < constants::PLAYER_SIZE) {
                    if (i != player_lane) {
                        action.target_lane = static_cast<int8_t>(i);
                    }
                    break;
                }
            }
        }
    }

    // Lane requirement (BlockedLanes — find unblocked)
    auto* blocked = reg.try_get<BlockedLanes>(entity);
    if (blocked) {
        int8_t safe = nearest_unblocked_lane(blocked->mask, player_lane);
        if (safe != player_lane) {
            action.target_lane = safe;
        }
    }

    // Lane requirement (RequiredLane — exact lane)
    auto* req_lane = reg.try_get<RequiredLane>(entity);
    if (req_lane && req_lane->lane != player_lane) {
        action.target_lane = req_lane->lane;
    }

    // Vertical requirement
    auto* req_v = reg.try_get<RequiredVAction>(entity);
    if (req_v) {
        action.target_vertical = req_v->action;
    }

    return action;
}

// ── System ───────────────────────────────────────────────────

void test_player_system(entt::registry& reg, float dt) {
    auto* state = reg.ctx().find<TestPlayerState>();
    if (!state || !state->active) return;

    state->frame_count++;
    state->clean_planned(reg);

    auto& gs    = reg.ctx().get<GameState>();
    auto& aq    = reg.ctx().get<ActionQueue>();
    auto* log   = reg.ctx().find<SessionLog>();
    auto* song  = reg.ctx().find<SongState>();
    float song_time = song ? song->song_time : 0.0f;

    if (log) log->frame = state->frame_count;

    // ── AUTO-START ───────────────────────────────────────────
    if (gs.phase == GamePhase::Title || gs.phase == GamePhase::GameOver ||
        gs.phase == GamePhase::SongComplete) {
        if (gs.phase_timer > 0.5f) {
            aq.tap(Button::Confirm);
            if (log) {
                const char* phase_name =
                    (gs.phase == GamePhase::Title) ? "Title" :
                    (gs.phase == GamePhase::SongComplete) ? "SongComplete" : "GameOver";
                session_log_write(*log, song_time, "PLAYER",
                    "AUTO_START phase=%s", phase_name);

                // Log song results before replaying
                if (gs.phase == GamePhase::SongComplete) {
                    auto* results = reg.ctx().find<SongResults>();
                    auto* score = reg.ctx().find<ScoreState>();
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

    if (gs.phase == GamePhase::Paused) {
        aq.tap(Button::Confirm);
        return;
    }

    if (gs.phase != GamePhase::Playing) return;
    if (!song) return;

    // Reset stale state when a new play session starts (after enter_playing
    // calls reg.clear(), all old entity IDs are invalid).
    if (song->song_time < 0.01f && state->action_count > 0) {
        state->action_count = 0;
        state->planned_count = 0;
    }

    const auto& cfg = state->config();

    // ── Find player ──────────────────────────────────────────
    auto player_view = reg.view<PlayerTag, Position, PlayerShape, ShapeWindow, Lane, VerticalState>();
    if (player_view.size_hint() == 0) return;

    auto player_entity = *player_view.begin();
    auto [p_pos, p_shape, p_window, p_lane, p_vstate] =
        player_view.get<Position, PlayerShape, ShapeWindow, Lane, VerticalState>(player_entity);

    // ── PERCEIVE: scan obstacles in vision range ─────────────
    // Compute the "effective lane" — where the player will be after
    // all pending lane changes in the action queue execute.
    int8_t effective_lane = p_lane.current;
    if (p_lane.target >= 0) effective_lane = p_lane.target;
    for (int i = 0; i < state->action_count; ++i) {
        if (state->actions[i].target_lane >= 0 && !state->actions[i].lane_done()) {
            effective_lane = state->actions[i].target_lane;
        }
    }

    auto obs_view = reg.view<ObstacleTag, Position, Obstacle>(entt::exclude<ScoredTag>);
    for (auto [entity, obs_pos, obs] : obs_view.each()) {
        float dist = p_pos.y - obs_pos.y;
        if (dist <= 0.0f || dist > cfg.vision_range) continue;
        if (state->is_planned(entity)) continue;

        TestPlayerAction action = determine_action(reg, entity, effective_lane, *song);

        // Roll reaction timer
        std::uniform_real_distribution<float> reaction_dist(cfg.reaction_min, cfg.reaction_max);
        float reaction = reaction_dist(state->rng);

        // Pro player aims for Perfect timing on SHAPE PRESSES.
        // Lane dodges and vertical-only actions react ASAP — no delay.
        bool has_shape = (action.target_shape != Shape::Hexagon);

        if (cfg.aim_perfect && has_shape) {
            float ideal_press = action.arrival_time - song->morph_duration - song->half_window;
            float time_until_ideal = ideal_press - song->song_time;
            if (time_until_ideal > cfg.reaction_min) {
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

        state->push_action(action);
        state->mark_planned(entity);

        if (log) {
            auto* beat_info = reg.try_get<BeatInfo>(entity);
            int beat_num = beat_info ? beat_info->beat_index : -1;

            session_log_write(*log, song_time, "PLAYER",
                "PERCEIVE obstacle=%u beat=%d kind=%s shape=%s lane=%d dist=%.0fpx",
                static_cast<unsigned>(entt::to_integral(entity)),
                beat_num,
                obstacle_kind_name(obs.kind),
                shape_name(action.target_shape),
                action.target_lane, dist);

            session_log_write(*log, song_time, "PLAYER",
                "PLAN action=%s%s%s react=%.3fs arrival=%.3fs",
                action.target_shape != Shape::Hexagon ? shape_name(action.target_shape) : "",
                action.target_lane >= 0 ? "+lane" : "",
                action.target_vertical != VMode::Grounded ?
                    (action.target_vertical == VMode::Jumping ? "+jump" : "+slide") : "",
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
    constexpr float COLLISION_MARGIN = 40.0f;

    // Track which obstacle has a pending shape press (pressed but not yet scored).
    // Lane changes for OTHER obstacles should wait, but lane changes for the
    // SAME obstacle are fine — they're part of clearing it.
    entt::entity pending_shape_obstacle = entt::null;
    for (int i = 0; i < state->action_count; ++i) {
        auto& a = state->actions[i];
        // Shape press fired but obstacle hasn't been scored yet.
        // scoring_system removes Obstacle component after processing.
        if (a.target_shape != Shape::Hexagon && a.shape_done()
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
        if (action.needs_shape()) {
            switch (action.target_shape) {
                case Shape::Circle:   aq.tap(Button::ShapeCircle); break;
                case Shape::Square:   aq.tap(Button::ShapeSquare); break;
                case Shape::Triangle: aq.tap(Button::ShapeTri); break;
                default: break;
            }
            action.mark_shape_done();
            key_injected = true;

            if (log) {
                session_log_write(*log, song_time, "PLAYER",
                    "EXECUTE %s for obstacle=%u beat=%d",
                    shape_key_name(action.target_shape),
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
            auto zone_view = reg.view<ObstacleTag, Position>(entt::exclude<ScoredTag>);
            for (auto [ze, zpos] : zone_view.each()) {
                if (ze == action.obstacle) continue; // don't self-block
                float zdist = p_pos.y - zpos.y + p_vstate.y_offset;
                if (zdist >= -COLLISION_MARGIN && zdist <= COLLISION_MARGIN * 3.0f) {
                    zone_blocked = true;
                    break;
                }
            }
        }

        // Would moving to target_lane cause a MISS on any CLOSER unresolved
        // obstacle? A human player would see "something is in the way" and
        // wait. Don't move for a far obstacle if it breaks a closer one.
        bool move_would_fail_closer = false;
        if (action.needs_lane()) {
            int8_t next_lane = p_lane.current;
            if (action.target_lane < next_lane) next_lane--;
            else if (action.target_lane > next_lane) next_lane++;

            auto closer_view = reg.view<ObstacleTag, Position>(entt::exclude<ScoredTag>);
            for (auto [oe, opos] : closer_view.each()) {
                if (oe == action.obstacle) continue;
                float odist = p_pos.y - opos.y + p_vstate.y_offset;
                if (odist <= 0.0f) continue;

                auto* obeat = reg.try_get<BeatInfo>(oe);
                float o_arrival = obeat ? obeat->arrival_time
                    : (song_time + odist / song->scroll_speed);
                if (o_arrival >= action.arrival_time) continue; // farther, don't care

                // Closer obstacle — check if proposed lane is safe for it
                auto* oblocked = reg.try_get<BlockedLanes>(oe);
                if (oblocked && ((oblocked->mask >> next_lane) & 1)) {
                    move_would_fail_closer = true;
                    break;
                }
                auto* oshape = reg.try_get<RequiredShape>(oe);
                if (oshape) {
                    float lane_x = constants::LANE_X[next_lane];
                    if (std::abs(opos.x - lane_x) >= constants::PLAYER_SIZE) {
                        move_would_fail_closer = true;
                        break;
                    }
                }
            }
        }

        if (action.needs_lane() && p_lane.target < 0 && state->swipe_cooldown_timer <= 0.0f
            && !zone_blocked && !blocked_by_shape && !move_would_fail_closer) {
            if (action.target_lane < p_lane.current) {
                aq.go(Direction::Left);
                if (log) {
                    session_log_write(*log, song_time, "PLAYER",
                        "EXECUTE Go(Left) for obstacle=%u beat=%d",
                        static_cast<unsigned>(entt::to_integral(action.obstacle)), act_beat);
                }
            } else if (action.target_lane > p_lane.current) {
                aq.go(Direction::Right);
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
                action.mark_lane_done();
            }
            key_injected = true;
            continue;
        }

        // Priority 3: Vertical action (wait for other obstacles to clear zone).
        // Reuses zone_blocked / blocked_by_shape computed above: we only reach
        // here when the lane branch did not fire (no mutating ops in between),
        // so those values are still valid for the same action.
        if (action.needs_vertical() && p_vstate.mode == VMode::Grounded
            && !zone_blocked && !blocked_by_shape) {
            if (action.target_vertical == VMode::Jumping) {
                aq.go(Direction::Up);
                if (log) {
                    session_log_write(*log, song_time, "PLAYER",
                        "EXECUTE Go(Up) for obstacle=%u beat=%d",
                        static_cast<unsigned>(entt::to_integral(action.obstacle)), act_beat);
                }
            } else if (action.target_vertical == VMode::Sliding) {
                aq.go(Direction::Down);
                if (log) {
                    session_log_write(*log, song_time, "PLAYER",
                        "EXECUTE Go(Down) for obstacle=%u beat=%d",
                        static_cast<unsigned>(entt::to_integral(action.obstacle)), act_beat);
                }
            }
            action.mark_vertical_done();
            key_injected = true;
            continue;
        }
    }

    // ── CLEANUP completed actions ────────────────────────────
    for (int i = state->action_count - 1; i >= 0; --i) {
        auto& action = state->actions[i];
        bool done = action.all_done();
        // Entity no longer an active obstacle (scored + processed by scoring_system,
        // or destroyed by cleanup_system)
        bool expired = !reg.valid(action.obstacle) ||
                       !reg.all_of<Obstacle>(action.obstacle);
        if (done || expired) {
            state->remove_action(i);
        }
    }
}
