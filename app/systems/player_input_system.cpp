#include "all_systems.h"
#include "../components/game_state.h"
#include "game_phase_transition.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "input_events.h"
#include "audio_events.h"
#include "haptics.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include "../util/lane_utils.h"
#include "../util/shape_lane_mapping.h"
#include "../util/shape_tag.h"

namespace {

bool gameplay_input_enabled(entt::registry& reg) {
    // Per Fabian's existential processing (issue #1202/#1204): dispatch on the
    // per-phase ctx-tag mirror; the pending-transition signal is the presence
    // of any `NextPhase*Tag` on `reg.ctx()` (`is_phase_transition_pending`).
    return reg.ctx().contains<GamePhasePlayingTag>() && !is_phase_transition_pending(reg);
}

void push_haptic(entt::registry& reg, HapticEvent event) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (disp) disp->enqueue<PlayHapticEvent>({event});
}

}  // namespace

namespace {

void try_lane_shift(entt::registry& reg, int8_t delta) {
    if (!gameplay_input_enabled(reg)) return;
    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {
        lane_utils::normalize(lane);
        const bool lane_switch_active =
            lane_utils::is_valid(lane.target) && lane.target != lane.current;
        if (lane_switch_active) {
            (void)pshape;
            (void)swindow;
            continue;
        }
        const int8_t next = static_cast<int8_t>(lane.current + delta);
        if (next < 0 || next >= constants::LANE_COUNT) {
            (void)pshape;
            (void)swindow;
            continue;
        }
        lane.target = next;
        lane.lerp_t = 0.0f;
        push_haptic(reg, HapticEvent::LaneSwitch);
        (void)pshape;
        (void)swindow;
    }
}

// Vertical inputs (jump/slide) are ignored when a horizontal lane switch is
// in flight — preserves the pre-#1279 guard (`horizontal_input` short-
// circuited the jump/slide arms when the player was committed to a swipe)
// and the "grounded" requirement (no `Jumping`/`Sliding` already on the
// player). Each kind has its own emplace below.
template <typename Op>
void try_vertical(entt::registry& reg, Op&& op) {
    if (!gameplay_input_enabled(reg)) return;
    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {
        const bool grounded = !reg.any_of<Jumping, Sliding>(entity);
        if (!grounded) {
            (void)pshape;
            (void)swindow;
            (void)lane;
            continue;
        }
        lane_utils::normalize(lane);
        const bool lane_switch_active =
            lane_utils::is_valid(lane.target) && lane.target != lane.current;
        if (lane_switch_active) {
            (void)pshape;
            (void)swindow;
            continue;
        }
        op(entity);
        (void)pshape;
        (void)swindow;
    }
}

}  // namespace

void player_input_handle_go_left(entt::registry& reg, const GoLeftEvent&) {
    try_lane_shift(reg, int8_t{-1});
}

void player_input_handle_go_right(entt::registry& reg, const GoRightEvent&) {
    try_lane_shift(reg, int8_t{+1});
}

void player_input_handle_go_up(entt::registry& reg, const GoUpEvent&) {
    try_vertical(reg, [&reg](entt::entity entity) {
        reg.emplace<Jumping>(entity, Jumping{constants::JUMP_DURATION, 0.0f});
    });
}

void player_input_handle_go_down(entt::registry& reg, const GoDownEvent&) {
    try_vertical(reg, [&reg](entt::entity entity) {
        reg.emplace<Sliding>(entity, Sliding{constants::SLIDE_DURATION});
    });
}

namespace {

void player_input_handle_shape_press_impl(entt::registry& reg, Shape pressed_shape) {
    if (!gameplay_input_enabled(reg)) return;
    auto* song = reg.ctx().find<SongState>();
    const bool rhythm_mode = (song != nullptr && (song->playing || song->finished));

    const int pressed_shape_index = shape_index(pressed_shape);
    if (pressed_shape_index < 0) return;  // unreachable: callers pass Circle/Square/Triangle

    auto begin_shape_window = [&](entt::entity entity, PlayerShape& ps, ShapeWindow& sw) {
        set_target_shape_tag(reg, entity, pressed_shape);
        sw.window_timer = 0.0f;
        sw.window_start = song->song_time;
        // Pressed row table presence == "this window has a press"; new press
        // also clears any prior grading flag (Fabian Principle 3, issue #1533).
        reg.emplace_or_replace<Pressed>(entity, Pressed{song->song_time});
        ps.morph_t = 0.0f;
        reg.remove<WindowGraded>(entity);
        // Phase transition: any previous phase → MorphIn (#1202/#1204).
        reg.remove<ShapeWindowActiveTag>(entity);
        reg.remove<ShapeWindowMorphOutTag>(entity);
        reg.emplace_or_replace<ShapeWindowMorphInTag>(entity);
        if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
            disp->enqueue<PlaySfxEvent>({SFX::ShapeShift});
        }
        push_haptic(reg, HapticEvent::ShapeShift);
    };

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {
        if (rhythm_mode) {
            const bool is_idle      = !reg.any_of<ShapeWindowMorphInTag,
                                                  ShapeWindowActiveTag,
                                                  ShapeWindowMorphOutTag>(entity);
            const bool is_active    = reg.all_of<ShapeWindowActiveTag>(entity);
            const bool is_morph_out = reg.all_of<ShapeWindowMorphOutTag>(entity);
            if (is_idle) {
                begin_shape_window(entity, pshape, swindow);
            } else if (is_active && pressed_shape != current_player_shape(reg, entity)) {
                begin_shape_window(entity, pshape, swindow);
            } else if (is_morph_out) {
                begin_shape_window(entity, pshape, swindow);
            }
        } else {
            if (pressed_shape != current_player_shape(reg, entity)) {
                set_player_shape_tag(reg, entity, pressed_shape);
                pshape.morph_t  = 1.0f;
                const auto& sc = constants::SHAPE_COLORS[pressed_shape_index];
                reg.replace<Color>(entity, sc);
                if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
                    disp->enqueue<PlaySfxEvent>({SFX::ShapeShift});
                }
                push_haptic(reg, HapticEvent::ShapeShift);
            }
        }
        (void)lane;
        (void)swindow;
    }
}

}  // namespace

void player_input_handle_press_circle(entt::registry& reg, const ShapePressCircleEvent&) {
    player_input_handle_shape_press_impl(reg, Shape::Circle);
}

void player_input_handle_press_square(entt::registry& reg, const ShapePressSquareEvent&) {
    player_input_handle_shape_press_impl(reg, Shape::Square);
}

void player_input_handle_press_triangle(entt::registry& reg, const ShapePressTriangleEvent&) {
    player_input_handle_shape_press_impl(reg, Shape::Triangle);
}
