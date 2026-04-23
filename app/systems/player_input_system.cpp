#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/input_events.h"
#include "../components/audio.h"
#include "../components/rhythm.h"
#include "../constants.h"

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
    };

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane, VerticalState>();
    for (auto [entity, pshape, swindow, lane, vstate] : view.each()) {

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
                }
            }
        }

        // Lane change from GoEvents
        for (int i = 0; i < eq.go_count; ++i) {
            auto dir = eq.goes[i].dir;
            if (dir == Direction::Left && lane.current > 0) {
                lane.target = lane.current - 1;
                lane.lerp_t = 0.0f;
            }
            if (dir == Direction::Right && lane.current < constants::LANE_COUNT - 1) {
                lane.target = lane.current + 1;
                lane.lerp_t = 0.0f;
            }
        }
    }
}
