#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rendering.h"
#include "../components/rhythm.h"
#include "../constants.h"

static void apply_shape_color(entt::registry& reg, entt::entity entity, Shape shape) {
    auto si = static_cast<int>(shape);
    auto& sc = constants::SHAPE_COLORS[si];
    reg.replace<DrawColor>(entity, sc.r, sc.g, sc.b, sc.a);
}

void shape_window_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();
    if (!song) return;

    auto view = reg.view<PlayerTag, PlayerShape, ShapeWindow, DrawColor>();
    for (auto [entity, pshape, swindow, col] : view.each()) {
        auto phase = static_cast<WindowPhase>(swindow.phase_raw);

        switch (phase) {
            case WindowPhase::Idle:
                break;

            case WindowPhase::MorphIn:
                swindow.window_timer += dt;
                pshape.morph_t = swindow.window_timer / song->morph_duration;
                if (pshape.morph_t >= 1.0f) {
                    pshape.morph_t = 1.0f;
                    swindow.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
                    swindow.window_timer = 0.0f;
                    pshape.current = swindow.target_shape;
                    apply_shape_color(reg, entity, pshape.current);
                }
                break;

            case WindowPhase::Active:
                swindow.window_timer += dt;
                {
                    float effective_duration = (swindow.window_scale > 1.0f)
                        ? song->window_duration * swindow.window_scale
                        : song->window_duration;
                    if (swindow.window_timer >= effective_duration) {
                        swindow.phase_raw = static_cast<uint8_t>(WindowPhase::MorphOut);
                        swindow.window_timer = 0.0f;
                        pshape.previous = pshape.current;
                        pshape.morph_t = 0.0f;
                    }
                }
                break;

            case WindowPhase::MorphOut:
                swindow.window_timer += dt;
                pshape.morph_t = swindow.window_timer / song->morph_duration;
                if (pshape.morph_t >= 1.0f) {
                    pshape.morph_t = 1.0f;
                    pshape.current = Shape::Hexagon;
                    pshape.previous = Shape::Hexagon;
                    swindow.target_shape = Shape::Hexagon;
                    swindow.phase_raw = static_cast<uint8_t>(WindowPhase::Idle);
                    swindow.window_timer = 0.0f;
                    swindow.window_scale = 1.0f;
                    swindow.graded = false;
                    apply_shape_color(reg, entity, Shape::Hexagon);
                }
                break;
        }
    }
}
