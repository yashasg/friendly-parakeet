#include "../components/audio.h"
#include "../components/game_state.h"
#include "../components/gameplay_intents.h"
#include "../components/registry_context.h"
#include "../entities/popup_entity.h"

#include <entt/entt.hpp>

void popup_feedback_system(entt::registry& reg, float /*dt*/) {
    auto* queue = registry_ctx_find<ScorePopupRequestQueue>(reg);
    if (!queue || queue->requests.empty()) return;
    const auto* gs = registry_ctx_find<GameState>(reg);
    if (!gs || !is_playing_phase(gs->phase)) {
        // Keep session-bound intent queues from leaking across phase transitions.
        queue->requests.clear();
        return;
    }

    auto* audio = registry_ctx_find<AudioQueue>(reg);
    for (const auto& request : queue->requests) {
        spawn_score_popup(reg, {request.x, request.y, request.points, request.timing_tier});
        if (audio) {
            audio->push(SFX::ScorePopup);
        }
    }
    queue->requests.clear();
}
