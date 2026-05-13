#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/input_events.h"
#include "../components/audio_events.h"
#include "../components/haptics.h"
#include "../components/rhythm.h"
#include "../entities/settings.h"
#include "../constants.h"

namespace {

bool gameplay_input_enabled(entt::registry& reg) {
    const auto& gs = reg.ctx().get<GameState>();
    return gs.phase == GamePhase::Playing && !gs.transition_pending;
}

void push_haptic(entt::registry& reg, HapticEvent event) {
    auto* disp = reg.ctx().find<entt::dispatcher>();
    if (disp) disp->enqueue<PlayHapticEvent>({event});
}

}  // namespace

void player_input_handle_go(entt::registry& reg, const GoEvent& evt) {
    if (!gameplay_input_enabled(reg)) return;
    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane, VerticalState>();
    for (auto [entity, pshape, swindow, lane, vstate] : view.each()) {
        int8_t delta = 0;
        if (evt.dir == Direction::Left  && lane.current > 0)
            delta = -1;
        else if (evt.dir == Direction::Right && lane.current < constants::LANE_COUNT - 1)
            delta = 1;
        if (delta != 0) {
            lane.target = lane.current + delta;
            lane.lerp_t = 0.0f;
            push_haptic(reg, HapticEvent::LaneSwitch);
        } else if (evt.dir == Direction::Up && vstate.mode == VMode::Grounded) {
            vstate.mode = VMode::Jumping;
            vstate.timer = constants::JUMP_DURATION;
            vstate.y_offset = 0.0f;
        } else if (evt.dir == Direction::Down && vstate.mode == VMode::Grounded) {
            vstate.mode = VMode::Sliding;
            vstate.timer = constants::SLIDE_DURATION;
            vstate.y_offset = 0.0f;
        }
        (void)entity;
        (void)pshape;
        (void)swindow;
    }
}

void player_input_handle_press(entt::registry& reg, const ButtonPressEvent& evt) {
    if (evt.kind != ButtonPressKind::Shape) return;  // ignore menu-button events
    if (!gameplay_input_enabled(reg)) return;
    auto* song = reg.ctx().find<SongState>();
    const bool rhythm_mode = (song != nullptr && (song->playing || song->finished));

    auto pressed_shape = evt.shape;

    auto begin_shape_window = [&](PlayerShape& ps, ShapeWindow& sw) {
        Shape previous_shape = ps.current;
        sw.target_shape = pressed_shape;
        ps.previous = previous_shape;
        sw.phase = WindowPhase::MorphIn;
        sw.window_timer = 0.0f;
        sw.window_start = song->song_time;
        sw.press_time = song->song_time;
        sw.peak_time = song->song_time + song->half_window;
        ps.morph_t = 0.0f;
        sw.window_scale = 1.0f;
        sw.graded = false;
        if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
            disp->enqueue<PlaySfxEvent>({SFX::ShapeShift});
        }
        push_haptic(reg, HapticEvent::ShapeShift);
    };

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {
        if (rhythm_mode) {
            auto phase = swindow.phase;
            if (phase == WindowPhase::Idle) {
                begin_shape_window(pshape, swindow);
            } else if (phase == WindowPhase::Active && pressed_shape != pshape.current) {
                begin_shape_window(pshape, swindow);
            } else if (phase == WindowPhase::MorphOut) {
                begin_shape_window(pshape, swindow);
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
        (void)lane;
    }
}
