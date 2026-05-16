#include "all_systems.h"

#include "audio_events.h"
#include "../components/game_state.h"
#include "gameplay_intents.h"
#include "../entities/popup_entity.h"

namespace {

// Common per-tier dispatch: drain one tier's queue via the matching spawner.
// SFX is emitted for any positive-points popup regardless of tier — the
// scoring SFX is a one-shot side-effect tied to "a popup spawned with points,"
// not to a particular tier, so it lives in the shared epilogue.
template <typename Request, typename Spawn>
void drain_timed_queue(entt::registry& reg,
                       std::vector<Request>& queue,
                       Spawn spawn,
                       entt::dispatcher* disp) {
    for (const auto& req : queue) {
        spawn(reg, req.x, req.y, req.points);
        if (disp && req.points > 0) disp->enqueue<PlaySfxEvent>({SFX::ScorePopup});
    }
    queue.clear();
}

}  // namespace

void popup_feedback_system(entt::registry& reg, [[maybe_unused]] float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;
    auto& queue = reg.ctx().get<ScorePopupRequestQueue>();
    if (queue.perfect.empty() && queue.good.empty() && queue.ok.empty() &&
        queue.bad.empty() && queue.untimed.empty()) {
        return;
    }

    auto* disp = reg.ctx().find<entt::dispatcher>();

    // Per-tier transforms — one per former TimingTier value (#1202/#1204).
    drain_timed_queue(reg, queue.perfect, spawn_score_popup_perfect, disp);
    drain_timed_queue(reg, queue.good,    spawn_score_popup_good,    disp);
    drain_timed_queue(reg, queue.ok,      spawn_score_popup_ok,      disp);
    drain_timed_queue(reg, queue.bad,     spawn_score_popup_bad,     disp);
    drain_timed_queue(reg, queue.untimed, spawn_score_popup_untimed, disp);
}
