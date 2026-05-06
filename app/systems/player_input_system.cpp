#include "../components/audio.h"
#include "../components/game_state.h"
#include "../components/registry_context.h"
#include "../components/player.h"
#include "../components/input_events.h"
#include "../util/haptic_queue.h"
#include "../components/rhythm.h"
#include "input_routing.h"
#include "input_latency_probe.h"
#include "../constants.h"

namespace {
bool gameplay_input_active(const entt::registry& reg) {
    const auto* gs = registry_ctx_find<GameState>(reg);
    return gs && is_playing_phase(gs->phase);
}

void queue_lane_switch(entt::registry& reg, Lane& lane, int8_t target_lane) {
    if (target_lane < 0 || target_lane >= constants::LANE_COUNT) return;
    if (lane.current == target_lane || lane.target == target_lane) return;
    lane.target = target_lane;
    lane.lerp_t = 0.0f;
    haptic_feedback(reg, HapticEvent::LaneSwitch);
}

void apply_shape_change_feedback(entt::registry& reg, entt::entity entity, Shape shape) {
    const auto shape_index = static_cast<int>(shape);
    reg.replace<SDL_Color>(entity, constants::SHAPE_COLORS[shape_index]);
    registry_ctx_get<AudioQueue>(reg).push(SFX::ShapeShift);
    haptic_feedback(reg, HapticEvent::ShapeShift);
}

void reset_shape_window_timing(ShapeWindow& sw, const SongState& song) {
    sw.window_timer = 0.0f;
    sw.window_start = song.song_time;
    sw.press_time = song.song_time;
    sw.peak_time = song.song_time + song.half_window;
    sw.window_scale = 1.0f;
    sw.graded = false;
}

int8_t lane_for_shape_button(Shape shape) {
    switch (shape) {
        case Shape::Circle:   return 0;
        case Shape::Square:   return 1;
        case Shape::Triangle: return 2;
        case Shape::Hexagon:  return -1;
    }
    return -1;
}
}  // namespace

void player_input_handle_go(entt::registry& reg, const GoEvent& evt) {
    input_latency_note_go_event_handled(reg, evt.dir);
    if (!gameplay_input_active(reg)) return;
    auto view = reg.view<PlayerTag, Lane>();
    for (auto [entity, lane] : view.each()) {
        if (evt.dir == Direction::Left && lane.current > 0) {
            queue_lane_switch(reg, lane, static_cast<int8_t>(lane.current - 1));
        } else if (evt.dir == Direction::Right && lane.current < constants::LANE_COUNT - 1) {
            queue_lane_switch(reg, lane, static_cast<int8_t>(lane.current + 1));
        }
        (void)entity;
    }
}

void player_input_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    if (evt.kind != ButtonPressKind::Shape) return;  // ignore menu-button events
    if (!gameplay_input_active(reg)) return;
    auto* song = registry_ctx_find<SongState>(reg);
    bool rhythm_mode = (song != nullptr && song->playing);

    auto pressed_shape = evt.shape;
    auto shape_lane = lane_for_shape_button(pressed_shape);

    auto begin_shape_window = [&](entt::entity entity, PlayerShape& ps, ShapeWindow& sw) {
        const Shape previous_shape = ps.current;
        sw.target_shape = pressed_shape;
        ps.previous = previous_shape;
        ps.current = pressed_shape;  // Instant swap: no MorphIn delay
        sw.phase = WindowPhase::Active;
        reset_shape_window_timing(sw, *song);
        ps.morph_t = 1.0f;
        apply_shape_change_feedback(reg, entity, pressed_shape);
    };

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {
        if (shape_lane >= 0) {
            queue_lane_switch(reg, lane, shape_lane);
        }
        if (rhythm_mode) {
            const auto phase = swindow.phase;
            if (phase == WindowPhase::Idle) {
                begin_shape_window(entity, pshape, swindow);
            } else if (phase == WindowPhase::Active && pressed_shape != pshape.current) {
                begin_shape_window(entity, pshape, swindow);
            } else if (phase == WindowPhase::Active && pressed_shape == pshape.current) {
                reset_shape_window_timing(swindow, *song);
            } else if (phase == WindowPhase::MorphOut) {
                begin_shape_window(entity, pshape, swindow);
            }
        } else {
            if (pressed_shape != pshape.current) {
                pshape.previous = pshape.current;
                pshape.current  = pressed_shape;
                pshape.morph_t  = 1.0f;
                apply_shape_change_feedback(reg, entity, pressed_shape);
            }
        }
    }
}

// ── EventQueue consumption contract ──────────────────────────────────────────
// player_input_handle_go and player_input_handle_press are connected as
// listeners in wire_input_dispatcher().  GoEvent and ButtonPressEvent are
// enqueued by input_system and swipe routing, then
// dispatched once per logical frame by the first consumer system that calls
// disp.update<T>() (game_state_system in the production tick order).
//
// In production, game_state_system is the single in-frame drain site.
// player_input_system is retained for isolated test harnesses that invoke only
// this system and still need listeners to fire.
//
// In isolated test scenarios where only player_input_system is invoked (no
// preceding game_state_system call), these update<T>() calls are the sole
// drain — they fire the listeners and deliver the enqueued events.  This dual
// role is intentional: update() on an empty queue is a defined no-op in EnTT,
// so the production path is never harmed by the defensive drain below.
//
// ⚠ Do NOT replace these update() calls with clear<T>() calls here.
//   clear() skips listeners entirely (EnTT R3), which would silently drop
//   events in the isolated-test path and mask regressions.
// ─────────────────────────────────────────────────────────────────────────────
void player_input_system(entt::registry& reg, float /*dt*/) {
    drain_semantic_input_events(reg);
}
