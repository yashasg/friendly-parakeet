#include "approach_ring_envelope_system.h"

#include "../components/game_state.h"
#include "../components/obstacle.h"
#include "../components/song_state.h"
#include "../components/transform.h"
#include "../components/ui.h"
#include "../constants.h"
#include "../entities/settings.h"
#include "../tags/tags.h"
#include "../util/gameplay_hud_ring.h"
#include "../util/shape_lane_mapping.h"
#include "../util/shape_tag.h"

#include <array>

namespace {

constexpr float kApproachRingMaxRadiusScale = 2.0f;

// Lane button is sized 140×100 in `content/ui/screens/gameplay.rgl`;
// dividing the width by 2.8 sizes the approach-ring radius to the
// visual scale the gameplay HUD is designed around.
constexpr float kLaneButtonRadiusDivisor = 2.8f;

template <typename ShapeTag>
void write_envelope_for(entt::registry& reg,
                        float nearest_dist,
                        float perfect_dist,
                        float good_dist,
                        float ok_dist,
                        bool  reduce_motion) {
    auto view = reg.view<LaneButtonTag, UiBounds, ShapeTag>();
    for (auto entity : view) {
        const auto& sz = view.template get<UiBounds>(entity);
        const float btn_radius = sz.w / kLaneButtonRadiusDivisor;
        const float max_ring_radius = btn_radius * kApproachRingMaxRadiusScale;
        const auto cue = gameplay_hud_ring_cue(nearest_dist, perfect_dist,
                                                good_dist, ok_dist);
        if (!cue.visible) {
            reg.remove<ApproachRing>(entity);
            reg.remove<ApproachRingVisibleTag>(entity);
            continue;
        }
        const float ratio = gameplay_hud_ring_ratio(nearest_dist, perfect_dist, ok_dist);
        const auto env = approach_ring_envelope(ratio, btn_radius,
                                                max_ring_radius, reduce_motion);
        if (env.alpha_scale <= 0.0f) {
            reg.remove<ApproachRing>(entity);
            reg.remove<ApproachRingVisibleTag>(entity);
            continue;
        }
        ApproachRing ring{};
        ring.radius      = env.radius;
        ring.alpha_scale = env.alpha_scale;
        ring.color_r     = cue.color.r;
        ring.color_g     = cue.color.g;
        ring.color_b     = cue.color.b;
        ring.color_a     = cue.color.a;
        reg.emplace_or_replace<ApproachRing>(entity, ring);
        reg.emplace_or_replace<ApproachRingVisibleTag>(entity);
    }
}

}  // namespace

void approach_ring_envelope_system(entt::registry& reg) {
    auto lane_view = reg.view<LaneButtonTag>();
    if (lane_view.begin() == lane_view.end()) return;
    if (!reg.ctx().contains<GamePhasePlayingTag>()) {
        // Clear envelope so the renderer doesn't draw a stale ring after
        // pause/resume cycles. Absence of `ApproachRingVisibleTag` (and
        // the optional `ApproachRing` row) is the "no ring" state.
        for (auto entity : lane_view) {
            reg.remove<ApproachRing>(entity);
            reg.remove<ApproachRingVisibleTag>(entity);
        }
        return;
    }

    // Nearest unscored obstacle per required-shape lane. Index matches
    // `shape_index(Shape)` from `shape_lane_mapping.h`. The fourth slot
    // (Hexagon) is read-once for parity with the legacy code but never
    // consumed because there is no Hexagon lane button.
    std::array<float, 4> nearest_dist = {-1.0f, -1.0f, -1.0f, -1.0f};
    for (auto [entity, obstacle, obstacle_pos] :
         reg.view<ObstacleTag, Obstacle, WorldPosition>(entt::exclude<ScoredTag>).each()) {
        (void)obstacle;
        if (!has_required_shape_tag(reg, entity)) continue;
        const int req_idx = shape_index(current_required_shape(reg, entity));
        if (req_idx < 0 || req_idx >= static_cast<int>(nearest_dist.size())) continue;
        const float dist = constants::PLAYER_Y - obstacle_pos.position.y;
        if (dist > 0.0f && (nearest_dist[req_idx] < 0.0f || dist < nearest_dist[req_idx])) {
            nearest_dist[req_idx] = dist;
        }
    }

    const auto* song_state = reg.ctx().find<SongState>();
    const float perfect_dist = gameplay_hud_perfect_distance(song_state);
    const float good_dist    = gameplay_hud_good_distance(song_state);
    const float ok_dist      = gameplay_hud_ok_distance(song_state);
    const auto* settings_ptr = find_settings_state(reg);
    const bool reduce_motion = settings_ptr && settings_ptr->reduce_motion;

    write_envelope_for<UiShapeIconCircleTag>  (reg,
        nearest_dist[shape_index(Shape::Circle)],   perfect_dist, good_dist, ok_dist, reduce_motion);
    write_envelope_for<UiShapeIconSquareTag>  (reg,
        nearest_dist[shape_index(Shape::Square)],   perfect_dist, good_dist, ok_dist, reduce_motion);
    write_envelope_for<UiShapeIconTriangleTag>(reg,
        nearest_dist[shape_index(Shape::Triangle)], perfect_dist, good_dist, ok_dist, reduce_motion);
}
