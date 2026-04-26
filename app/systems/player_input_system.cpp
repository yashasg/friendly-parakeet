#include "all_systems.h"
#include "burnout_helpers.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/input_events.h"
#include "../components/audio.h"
#include "../components/haptics.h"
#include "../components/settings.h"
#include "../components/rhythm.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/transform.h"
#include "../components/difficulty.h"
#include "../constants.h"
#include <limits>

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

    // Find nearest unscored obstacle ahead of the player matching kind_pred.
    auto find_nearest_unscored = [&](float player_y, auto kind_pred) -> entt::entity {
        float best_dist = std::numeric_limits<float>::max();
        entt::entity best = entt::null;
        auto obs_view = reg.view<ObstacleTag, Position, Obstacle>(entt::exclude<ScoredTag>);
        for (auto [e, opos, obs] : obs_view.each()) {
            if (!kind_pred(obs.kind)) continue;
            float dist = player_y - opos.y;
            if (dist > 0.0f && dist < best_dist) {
                best_dist = dist;
                best = e;
            }
        }
        return best;
    };

    // Snapshot BurnoutZone/multiplier on target at press-time distance.
    // First-commit-locks: does not overwrite an existing bank.
    auto bank_burnout = [&](entt::entity target, float player_y) {
        if (target == entt::null || !reg.valid(target)) return;
        if (reg.any_of<BankedBurnout>(target)) return;
        auto* opos = reg.try_get<Position>(target);
        if (!opos) return;
        float dist = player_y - opos->y;
        if (dist <= 0.0f) return;
        float scale = reg.ctx().get<DifficultyConfig>().burnout_window_scale;
        auto sample = compute_burnout_for_distance(dist, scale);
        float mult  = multiplier_for_zone(sample.zone);
        reg.emplace<BankedBurnout>(target, mult, sample.zone);
    };

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Lane, VerticalState, Position>();
    for (auto [entity, pshape, swindow, lane, vstate, ppos] : view.each()) {

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

            // Bank burnout on the nearest unscored shape-relevant obstacle.
            auto target = find_nearest_unscored(ppos.y, [](ObstacleKind k) {
                return k == ObstacleKind::ShapeGate ||
                       k == ObstacleKind::ComboGate ||
                       k == ObstacleKind::SplitPath;
            });
            bank_burnout(target, ppos.y);
        }

        // GoEvents: lane changes and jump/slide
        for (int i = 0; i < eq.go_count; ++i) {
            auto dir = eq.goes[i].dir;
            if (dir == Direction::Left && lane.current > 0) {
                lane.target = lane.current - 1;
                lane.lerp_t = 0.0f;
                {
                    auto* hq = reg.ctx().find<HapticQueue>();
                    auto* st = reg.ctx().find<SettingsState>();
                    if (hq) haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::LaneSwitch);
                }
                auto target = find_nearest_unscored(ppos.y, [](ObstacleKind k) {
                    return k == ObstacleKind::LaneBlock ||
                           k == ObstacleKind::ComboGate ||
                           k == ObstacleKind::SplitPath;
                });
                bank_burnout(target, ppos.y);
            }
            if (dir == Direction::Right && lane.current < constants::LANE_COUNT - 1) {
                lane.target = lane.current + 1;
                lane.lerp_t = 0.0f;
                {
                    auto* hq = reg.ctx().find<HapticQueue>();
                    auto* st = reg.ctx().find<SettingsState>();
                    if (hq) haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::LaneSwitch);
                }
                auto target = find_nearest_unscored(ppos.y, [](ObstacleKind k) {
                    return k == ObstacleKind::LaneBlock ||
                           k == ObstacleKind::ComboGate ||
                           k == ObstacleKind::SplitPath;
                });
                bank_burnout(target, ppos.y);
            }
        }

    }

    // Consume action events so the fixed-step accumulator's subsequent sub-ticks
    // don't replay the same GoEvents and ButtonPressEvents. Without this, a
    // two-tick frame resets lane.lerp_t to 0.0f on every sub-tick. (#213)
    eq.go_count    = 0;
    eq.press_count = 0;
}
