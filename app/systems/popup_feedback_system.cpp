#include "all_systems.h"

#include "../audio/audio_queue.h"
#include "../components/game_state.h"
#include "../components/gameplay_intents.h"
#include "../entities/popup_entity.h"

void popup_feedback_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* queue = reg.ctx().find<ScorePopupRequestQueue>();
    if (!queue || queue->requests.empty()) return;

    auto& audio = reg.ctx().get<AudioQueue>();
    for (const auto& request : queue->requests) {
        spawn_score_popup(reg, {request.x, request.y, request.points, request.timing_tier});
        audio_push(audio, SFX::ScorePopup);
    }
    queue->requests.clear();
}
