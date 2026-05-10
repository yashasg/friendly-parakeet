#include "all_systems.h"

#include "../components/audio_events.h"
#include "../components/game_state.h"
#include "../components/gameplay_intents.h"
#include "../entities/popup_entity.h"

void popup_feedback_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;
    auto& queue = reg.ctx().get<ScorePopupRequestQueue>();
    if (queue.requests.empty()) return;

    auto* disp = reg.ctx().find<entt::dispatcher>();
    for (const auto& request : queue.requests) {
        spawn_score_popup(reg, {request.x, request.y, request.points, request.timing_tier});
        if (disp) disp->enqueue<PlaySfxEvent>({SFX::ScorePopup});
    }
    queue.requests.clear();
}
