#pragma once

#include <entt/entt.hpp>

#include "../components/rhythm.h"
#include "../tags/tags.h"

// Per-row value for one pending energy delta (issue #1627). Attached to
// an entity carrying `PendingEnergyEffectTag` (+ optional `EnergyFlashTag`)
// by `scoring_system` during the miss/hit passes; drained and destroyed by
// `energy_system` at the start of the same frame. Replaces the former
// `PendingEnergyEffects::events` `std::vector<Event>` array column
// (Fabian Principle 3 — no array columns).
struct EnergyDelta {
    float value = 0.0f;
};

// Per-row value for one score popup request (issue #1626). Attached to
// an entity carrying exactly one of the `PopupRequestTier*Tag`s in
// `app/tags/tags.h` by `scoring_system` during the hit-pass drain;
// drained and destroyed by `popup_feedback_system` at end of frame.
// Replaces the former `ScorePopupRequestQueue`'s 5 inline
// `std::vector<TimedPopupRequest|UntimedPopupRequest>` per-tier columns
// (Fabian Principle 3 — no array columns).
//
// The former `TimedPopupRequest` / `UntimedPopupRequest` split was
// purely cosmetic — identical schema, named per destination queue. Now
// that the destination is a tag, the two row types collapse into one.
struct PopupRequest {
    float x      = 0.0f;
    float y      = 0.0f;
    int   points = 0;
};

// Spawn one popup-request row. `PopupRequestTierTag` is one of the per-tier
// tags in `app/tags/tags.h` (`PopupRequestTier{Perfect,Good,Ok,Bad,Untimed}Tag`);
// it selects which `popup_feedback_system` drain transform consumes the row.
template <typename PopupRequestTierTag>
inline entt::entity enqueue_popup_request(entt::registry& reg,
                                          float x, float y, int points) {
    auto e = reg.create();
    reg.emplace<PopupRequestTierTag>(e);
    reg.emplace<PopupRequest>(e, PopupRequest{x, y, points});
    return e;
}
