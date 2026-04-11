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

    auto view = reg.view<PlayerTag, PlayerShape, DrawColor>();
    for (auto [entity, pshape, col] : view.each()) {
        auto phase = static_cast<WindowPhase>(pshape.phase_raw);

        switch (phase) {
            case WindowPhase::Idle:
                break;

            case WindowPhase::MorphIn:
                pshape.window_timer += dt;
                pshape.morph_t = pshape.window_timer / song->morph_duration;
                if (pshape.morph_t >= 1.0f) {
                    pshape.morph_t = 1.0f;
                    pshape.phase_raw = static_cast<uint8_t>(WindowPhase::Active);
                    pshape.window_timer = 0.0f;
                    pshape.current = pshape.target_shape;
                    apply_shape_color(reg, entity, pshape.current);
                }
                break;

            case WindowPhase::Active:
                pshape.window_timer += dt;
                // window_scale > 1.0 extends the active phase (Perfect holds shape longer)
                // window_scale < 1.0 was already applied by advancing timer in collision
                {
                    float effective_duration = song->window_duration * pshape.window_scale;
                    if (pshape.window_timer >= effective_duration) {
                        pshape.phase_raw = static_cast<uint8_t>(WindowPhase::MorphOut);
                        pshape.window_timer = 0.0f;
                        pshape.previous = pshape.current;
                        pshape.morph_t = 0.0f;
                    }
                }
                break;

            case WindowPhase::MorphOut:
                pshape.window_timer += dt;
                pshape.morph_t = pshape.window_timer / song->morph_duration;
                if (pshape.morph_t >= 1.0f) {
                    pshape.morph_t = 1.0f;
                    pshape.current = Shape::Hexagon;
                    pshape.previous = Shape::Hexagon;
                    pshape.target_shape = Shape::Hexagon;
                    pshape.phase_raw = static_cast<uint8_t>(WindowPhase::Idle);
                    pshape.window_timer = 0.0f;
                    apply_shape_color(reg, entity, Shape::Hexagon);
                }
                break;
        }
    }
}
