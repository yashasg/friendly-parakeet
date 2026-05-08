#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/input_events.h"
#include "../audio/audio_types.h"
#include "../components/haptics.h"
#include "../components/rhythm.h"
#include "../util/settings.h"
#include "../constants.h"

namespace {

void push_haptic(entt::registry& reg, HapticEvent event) {
    auto* haptics = reg.ctx().find<HapticQueue>();
    auto* settings = reg.ctx().find<SettingsState>();
    if (!haptics || (settings && !settings->haptics_enabled) ||
        haptics->count >= HapticQueue::MAX_QUEUED) {
        return;
    }
    haptics->queue[haptics->count++] = event;
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
        auto& audio = reg.ctx().get<AudioQueue>();
        if (audio.count < AudioQueue::MAX_QUEUED) {
            audio.queue[audio.count++] = SFX::ShapeShift;
        }
        push_haptic(reg, HapticEvent::ShapeShift);
    };

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {
        if (shape_lane >= 0 && lane.current != shape_lane && lane.target != shape_lane) {
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
                auto& audio = reg.ctx().get<AudioQueue>();
                if (audio.count < AudioQueue::MAX_QUEUED) {
                    audio.queue[audio.count++] = SFX::ShapeShift;
                }
                push_haptic(reg, HapticEvent::ShapeShift);
            }
        }
    }
}

// ── EventQueue consumption contract ──────────────────────────────────────────
// player_input_handle_go and player_input_handle_press are connected as
// listeners in wire_input_dispatcher().  GoEvent and ButtonPressEvent are
// enqueued by input_system, gesture_routing, and raygui HUD controllers, then
// dispatched once per logical frame by the first consumer system that calls
// disp.update<T>() (game_state_system in the production tick order).
//
// In production the queue is already empty by the time this function runs, so
// the two update<T>() calls below are no-ops — enforcing the #213 no-replay
// invariant without extra bookkeeping.
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
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.update<GoEvent>();
    disp.update<ButtonPressEvent>();
}
