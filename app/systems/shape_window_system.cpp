#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rhythm.h"
#include "../constants.h"

void shape_window_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();
    if (!song) return;

    auto view = reg.view<PlayerTag, PlayerShape>();
    for (auto [entity, pshape] : view.each()) {
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
                }
                break;

            case WindowPhase::Active:
                pshape.window_timer += dt;
                if (pshape.window_timer >= song->window_duration) {
                    pshape.phase_raw = static_cast<uint8_t>(WindowPhase::MorphOut);
                    pshape.window_timer = 0.0f;
                    pshape.previous = pshape.current;
                    pshape.morph_t = 0.0f;
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
                }
                break;
        }
    }
}
