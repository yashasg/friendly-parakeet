#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/rhythm.h"
#include "../constants.h"

namespace {

static void apply_shape_color(entt::registry& reg, entt::entity entity, Shape shape) {
    auto si = static_cast<int>(shape);
    auto& sc = constants::SHAPE_COLORS[si];
    reg.replace<Color>(entity, sc);
}

void update_morph_in(entt::registry& reg,
                     entt::entity entity,
                     PlayerShape& pshape,
                     ShapeWindow& swindow,
                     const SongState& song) {
    const float elapsed = song.song_time - swindow.window_start;
    swindow.window_timer = elapsed;
    if (song.morph_duration <= 0.0f) {
        TraceLog(LOG_WARNING, "shape_window_system completed MorphIn immediately: invalid morph_duration %.3f",
                 song.morph_duration);
        pshape.morph_t = 1.0f;
        swindow.phase = WindowPhase::Active;
        swindow.window_start = song.song_time;
        swindow.window_timer = 0.0f;
        pshape.current = swindow.target_shape;
        apply_shape_color(reg, entity, pshape.current);
        return;
    }

    pshape.morph_t = elapsed / song.morph_duration;
    if (pshape.morph_t >= 1.0f) {
        pshape.morph_t = 1.0f;
        swindow.phase = WindowPhase::Active;
        swindow.window_start = swindow.window_start + song.morph_duration;
        swindow.window_timer = 0.0f;
        pshape.current = swindow.target_shape;
        apply_shape_color(reg, entity, pshape.current);
    }
}

}  // namespace

void shape_window_activation_system(entt::registry& reg, float /*dt*/) {
    auto* song = reg.ctx().find<SongState>();
    if (!song) return;

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Color>();
    for (auto [entity, pshape, swindow, col] : view.each()) {
        (void)col;
        if (swindow.phase == WindowPhase::MorphIn) {
            update_morph_in(reg, entity, pshape, swindow, *song);
        }
    }
}

void shape_window_system(entt::registry& reg, float /*dt*/) {
    auto* song = reg.ctx().find<SongState>();
    if (!song) return;

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, Color>();
    for (auto [entity, pshape, swindow, col] : view.each()) {
        (void)col;
        // Derive window_timer from song_time instead of accumulating dt.
        // This keeps shape windows frame-rate independent and perfectly
        // synced to the audio clock, per the "only use song position" rule.
        switch (swindow.phase) {
            case WindowPhase::Idle:
                break;

            case WindowPhase::MorphIn: {
                update_morph_in(reg, entity, pshape, swindow, *song);
                break;
            }

            case WindowPhase::Active: {
                float active_elapsed = song->song_time - swindow.window_start;
                swindow.window_timer = active_elapsed;
                float effective_duration = song->window_duration;
                if (active_elapsed >= effective_duration) {
                    swindow.phase = WindowPhase::MorphOut;
                    swindow.window_start = swindow.window_start + effective_duration;
                    swindow.window_timer = 0.0f;
                    pshape.previous = pshape.current;
                    pshape.morph_t = 0.0f;
                }
                break;
            }

            case WindowPhase::MorphOut: {
                float morph_elapsed = song->song_time - swindow.window_start;
                swindow.window_timer = morph_elapsed;
                if (song->morph_duration <= 0.0f) {
                    TraceLog(LOG_WARNING, "shape_window_system completed MorphOut immediately: invalid morph_duration %.3f",
                             song->morph_duration);
                    pshape.morph_t = 1.0f;
                    pshape.current = Shape::Hexagon;
                    pshape.previous = Shape::Hexagon;
                    swindow.target_shape = Shape::Hexagon;
                    swindow.phase = WindowPhase::Idle;
                    swindow.window_timer = 0.0f;
                    swindow.press_time = -1.0f;
                    swindow.window_scale = 1.0f;
                    swindow.graded = false;
                    apply_shape_color(reg, entity, Shape::Hexagon);
                    break;
                }
                pshape.morph_t = morph_elapsed / song->morph_duration;
                if (pshape.morph_t >= 1.0f) {
                    pshape.morph_t = 1.0f;
                    pshape.current = Shape::Hexagon;
                    pshape.previous = Shape::Hexagon;
                    swindow.target_shape = Shape::Hexagon;
                    swindow.phase = WindowPhase::Idle;
                    swindow.window_timer = 0.0f;
                    swindow.press_time = -1.0f;
                    swindow.window_scale = 1.0f;
                    swindow.graded = false;
                    apply_shape_color(reg, entity, Shape::Hexagon);
                }
                break;
            }
        }
    }
}
