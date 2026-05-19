#include "all_systems.h"

#include "audio_events.h"
#include "../components/game_state.h"
#include "gameplay_intents.h"
#include "../entities/popup_entity.h"
#include "../tags/tags.h"

namespace {

// Per-tier drain transform. Walks one popup-request row-table view
// (#1626 / Fabian Principle 3): each request entity carries
// `PopupRequest` (x, y, points) + one `PopupRequestTier*Tag`. After
// spawning the popup and emitting SFX, the request entity is destroyed.
// SFX is emitted for any positive-points popup regardless of tier — the
// scoring SFX is a one-shot side-effect tied to "a popup spawned with points,"
// not to a particular tier, so it lives in the shared epilogue.
template <typename TierTag, typename Spawn>
void drain_popup_requests(entt::registry& reg,
                          Spawn spawn,
                          entt::dispatcher* disp) {
    auto v = reg.view<PopupRequest, TierTag>();
    if (v.size_hint() == 0u) return;
    for (auto [e, req] : v.each()) {
        spawn(reg, req.x, req.y, req.points);
        if (disp && req.points > 0) disp->enqueue<PlaySfxEvent>({SFX::ScorePopup});
    }
    reg.destroy(v.begin(), v.end());
}

}  // namespace

void popup_feedback_system(entt::registry& reg, [[maybe_unused]] float dt) {
    if (!reg.ctx().contains<GamePhasePlayingTag>()) return;

    auto* disp = reg.ctx().find<entt::dispatcher>();

    // Per-tier transforms — one per former TimingTier value (#1202/#1204).
    drain_popup_requests<PopupRequestTierPerfectTag>(reg, spawn_score_popup_perfect, disp);
    drain_popup_requests<PopupRequestTierGoodTag>   (reg, spawn_score_popup_good,    disp);
    drain_popup_requests<PopupRequestTierOkTag>     (reg, spawn_score_popup_ok,      disp);
    drain_popup_requests<PopupRequestTierBadTag>    (reg, spawn_score_popup_bad,     disp);
    drain_popup_requests<PopupRequestTierUntimedTag>(reg, spawn_score_popup_untimed, disp);
}
