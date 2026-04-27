#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/input_events.h"
#include "../components/audio.h"
#include "../components/haptics.h"
#include "../components/settings.h"
#include "../components/rhythm.h"
#include "../constants.h"

// ── EventQueue consumption contract ──────────────────────────────────────────
// The fixed-step accumulator in game_loop.cpp may invoke the tick systems
// multiple times per render frame (once per accumulated dt step).
//
// player_input_system is the SOLE consumer of EventQueue action events:
//   • eq.goes[0..go_count-1]    — directional GoEvents (lane change, jump/slide)
//   • eq.presses[0..press_count-1] — ButtonPressEvents (shape buttons)
//
// After processing, it clears both queues (go_count = 0, press_count = 0) so
// that subsequent sub-ticks within the same render frame do NOT replay the
// same events (see #213 fix in commit 7b420ed).
//
// CONTRACT: No system that executes after player_input_system within the same
// fixed-step tick may read eq.go_count or eq.press_count and expect non-zero
// values. If a future system needs these queues it must either:
//   (a) run before player_input_system in the tick order, or
//   (b) be promoted to its own dedicated event-flush system that owns clearing.
// ─────────────────────────────────────────────────────────────────────────────

void player_input_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto& eq = reg.ctx().get<EventQueue>();

    auto* song = reg.ctx().find<SongState>();
    bool rhythm_mode = (song != nullptr && song->playing);

    auto begin_shape_window = [&](PlayerShape& ps, ShapeWindow& sw, Shape shape) {
        sw.target_shape = shape;
        ps.previous = ps.current;
        sw.phase_raw = static_cast<uint8_t>(WindowPhase::MorphIn);
        sw.window_timer = 0.0f;
        sw.window_start = song->song_time;
        sw.peak_time = song->song_time + song->morph_duration + song->half_window;
        ps.morph_t = 0.0f;
        sw.window_scale = 1.0f;
        sw.graded = false;
        audio_push(reg.ctx().get<AudioQueue>(), SFX::ShapeShift);
        {
            auto* hq = reg.ctx().find<HapticQueue>();
            auto* st = reg.ctx().find<SettingsState>();
            if (hq) haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::ShapeShift);
        }
    };

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane>();
    for (auto [entity, pshape, swindow, lane] : view.each()) {

        // Shape changes from ButtonPressEvents
        for (int i = 0; i < eq.press_count; ++i) {
            auto btn_entity = eq.presses[i].entity;
            if (!reg.valid(btn_entity)) continue;
            if (!reg.all_of<ShapeButtonTag, ShapeButtonData>(btn_entity)) continue;
            auto pressed_shape = reg.get<ShapeButtonData>(btn_entity).shape;

            if (rhythm_mode) {
                auto phase = static_cast<WindowPhase>(swindow.phase_raw);

                if (phase == WindowPhase::Idle) {
                    begin_shape_window(pshape, swindow, pressed_shape);
                } else if (phase == WindowPhase::Active && pressed_shape != pshape.current) {
                    begin_shape_window(pshape, swindow, pressed_shape);
                } else if (phase == WindowPhase::Active && pressed_shape == pshape.current) {
                    swindow.window_timer = 0.0f;
                    swindow.window_start = song->song_time;
                    swindow.peak_time = song->song_time + song->half_window;
                    swindow.window_scale = 1.0f;
                    swindow.graded = false;
                } else if (phase == WindowPhase::MorphOut) {
                    // Player pressed during MorphOut (returning to Hexagon).
                    // Accept and interrupt — preserve player intent rather than
                    // silently dropping the input. (#209)
                    begin_shape_window(pshape, swindow, pressed_shape);
                }
            } else {
                if (pressed_shape != pshape.current) {
                    pshape.previous = pshape.current;
                    pshape.current  = pressed_shape;
                    pshape.morph_t  = 0.0f;
                    auto si = static_cast<int>(pressed_shape);
                    auto& sc = constants::SHAPE_COLORS[si];
                    reg.replace<Color>(entity, sc);
                    audio_push(reg.ctx().get<AudioQueue>(), SFX::ShapeShift);
                    {
                        auto* hq = reg.ctx().find<HapticQueue>();
                        auto* st = reg.ctx().find<SettingsState>();
                        if (hq) haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::ShapeShift);
                    }
                }
            }

        } // end shape button loop

        // GoEvents: lane changes and jump/slide
        for (int i = 0; i < eq.go_count; ++i) {
            auto dir = eq.goes[i].dir;

            // Determine delta: -1 for Left, +1 for Right, 0 if at boundary.
            int8_t delta = 0;
            if (dir == Direction::Left  && lane.current > 0)
                delta = -1;
            else if (dir == Direction::Right && lane.current < constants::LANE_COUNT - 1)
                delta = 1;

            if (delta != 0) {
                lane.target = lane.current + delta;
                lane.lerp_t = 0.0f;
                {
                    auto* hq = reg.ctx().find<HapticQueue>();
                    auto* st = reg.ctx().find<SettingsState>();
                    if (hq) haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::LaneSwitch);
                }
            }
        }

    }

    // Consume action events so the fixed-step accumulator's subsequent sub-ticks
    // don't replay the same GoEvents and ButtonPressEvents. Without this, a
    // two-tick frame resets lane.lerp_t to 0.0f on every sub-tick. (#213)
    eq.go_count    = 0;
    eq.press_count = 0;
}
