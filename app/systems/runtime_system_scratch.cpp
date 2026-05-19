#include "all_systems.h"
#include "camera_system.h"
#include "game_state_system.h"
#include "gameplay_intents.h"
#include "../tags/tags.h"
#include "../components/rendering.h"

#include <cstddef>

void runtime_system_scratch_init(entt::registry& reg) {
    // WasmSmokeLastLane uses row presence as the "lane reported" predicate
    // (Fabian Principle 3 — no sentinel NULL columns). Erase any stale row
    // carried over from a prior session so the next title update rewrites
    // unconditionally.
    reg.ctx().erase<WasmSmokeLastLane>();

    // Destroy any stale popup request rows carried over from a prior
    // session (issue #1626 row-table migration). The former
    // `ScorePopupRequestQueue` ctx-vector clear was implicit here via
    // `insert_or_assign(ScorePopupRequestQueue{})`.
    auto stale = reg.view<PopupRequest>();
    reg.destroy(stale.begin(), stale.end());

    runtime_system_scratch_reserve(reg, 0);
}

void runtime_system_scratch_reserve(entt::registry& /*reg*/, std::size_t /*beat_capacity*/) {
    // PendingEnergyEffects (#1627), ScoringSystemScratch (#1629), and
    // ScorePopupRequestQueue (#1626) were all eradicated into per-frame
    // row tables. entt's storage grows naturally as `scoring_system`
    // emplaces `PendingMissResolveTag` / `PendingHitResolveTag` /
    // `PendingNonScorableCleanupTag` / `PendingEnergyEffectTag` /
    // `PopupRequestTier*Tag` rows; no caller-driven reserve is needed.
    //
    // The reserve symbol is retained as a no-op so session-init callers
    // (`game_loop_init`, `play_session`) and tests still compile against
    // their existing signatures.
}
