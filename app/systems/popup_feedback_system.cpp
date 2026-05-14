#include "all_systems.h"

#include "../components/audio_events.h"
#include "../components/game_state.h"
#include "../components/gameplay_intents.h"
#include "../entities/popup_entity.h"
#include <optional>

void popup_feedback_system(entt::registry& reg, [[maybe_unused]] float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;
    auto& queue = reg.ctx().get<ScorePopupRequestQueue>();
    if (queue.requests.empty()) return;

    auto* disp = reg.ctx().find<entt::dispatcher>();
    for (const auto& request : queue.requests) {
        const std::optional<TimingTier> timing_tier = request.has_timing_tier
            ? std::make_optional(request.timing_tier)
            : std::nullopt;
        spawn_score_popup(reg, {request.x, request.y, request.points, timing_tier});
        if (disp && request.points > 0) disp->enqueue<PlaySfxEvent>({SFX::ScorePopup});
    }
    queue.requests.clear();
}
