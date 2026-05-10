#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/input_events.h"
#include "../components/audio_events.h"
#include "../components/haptics.h"
#include "../components/rhythm.h"
#include "../util/settings.h"
#include "../constants.h"

namespace {

void push_haptic(entt::registry& reg, HapticEvent event) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (disp) disp->enqueue<PlayHapticEvent>({event});
}

}  // namespace

static int8_t lane_for_shape_button(Shape shape) {
    switch (shape) {
        case Shape::Circle:   return 0;
        case Shape::Square:   return 1;
        case Shape::Triangle: return 2;
        case Shape::Hexagon:  return -1;
    }
    return -1;
}

void player_input_handle_go(entt::registry& reg, const GoEvent& evt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;
    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {
        int8_t delta = 0;
        if (evt.dir == Direction::Left  && lane.current > 0)
            delta = -1;
        else if (evt.dir == Direction::Right && lane.current < constants::LANE_COUNT - 1)
            delta = 1;
        if (delta != 0) {
            lane.target = lane.current + delta;
            lane.lerp_t = 0.0f;
            push_haptic(reg, HapticEvent::LaneSwitch);
        }
        (void)entity;
        (void)pshape;
        (void)swindow;
    }
}

void player_input_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    if (evt.kind != ButtonPressKind::Shape) return;  // ignore menu-button events
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;
    auto* song = reg.ctx().find<SongState>();
    bool rhythm_mode = (song != nullptr && song->playing);

    auto pressed_shape = evt.shape;
    auto shape_lane = lane_for_shape_button(pressed_shape);

    auto begin_shape_window = [&](entt::entity entity, PlayerShape& ps, ShapeWindow& sw) {
        Shape previous_shape = ps.current;
        sw.target_shape = pressed_shape;
        ps.previous = previous_shape;
        ps.current = pressed_shape;  // Instant swap: no MorphIn delay
        sw.phase = WindowPhase::Active;
        sw.window_timer = 0.0f;
        sw.window_start = song->song_time;
        sw.press_time = song->song_time;
        sw.peak_time = song->song_time + song->half_window;
        ps.morph_t = 1.0f;
        sw.window_scale = 1.0f;
        sw.graded = false;
        auto si = static_cast<int>(pressed_shape);
        auto& sc = constants::SHAPE_COLORS[si];
        reg.replace<Color>(entity, sc);
        if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
            disp->enqueue<PlaySfxEvent>({SFX::ShapeShift});
        }
        push_haptic(reg, HapticEvent::ShapeShift);
    };

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {
        const bool not_in_shape_lane = (lane.current != shape_lane);
        const bool conflicting_in_flight_target =
            (lane.current == shape_lane && lane.target >= 0 && lane.target != shape_lane);
        if (shape_lane >= 0 && lane.target != shape_lane &&
            (not_in_shape_lane || conflicting_in_flight_target)) {
            lane.target = shape_lane;
            lane.lerp_t = 0.0f;
            push_haptic(reg, HapticEvent::LaneSwitch);
        }
        if (rhythm_mode) {
            auto phase = swindow.phase;
            if (phase == WindowPhase::Idle) {
                begin_shape_window(entity, pshape, swindow);
            } else if (phase == WindowPhase::Active && pressed_shape != pshape.current) {
                begin_shape_window(entity, pshape, swindow);
            } else if (phase == WindowPhase::Active && pressed_shape == pshape.current) {
                swindow.window_timer = 0.0f;
                swindow.window_start = song->song_time;
                swindow.press_time = song->song_time;
                swindow.peak_time = song->song_time + song->half_window;
                swindow.window_scale = 1.0f;
                swindow.graded = false;
            } else if (phase == WindowPhase::MorphOut) {
                begin_shape_window(entity, pshape, swindow);
            }
        } else {
            if (pressed_shape != pshape.current) {
                pshape.previous = pshape.current;
                pshape.current  = pressed_shape;
                pshape.morph_t  = 1.0f;
                auto si = static_cast<int>(pressed_shape);
                auto& sc = constants::SHAPE_COLORS[si];
                reg.replace<Color>(entity, sc);
                if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
                    disp->enqueue<PlaySfxEvent>({SFX::ShapeShift});
                }
                push_haptic(reg, HapticEvent::ShapeShift);
            }
        }
    }
}
