#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rhythm.h"
#include "../components/transform.h"
#include "audio_events.h"
#include "haptics.h"
#include "../constants.h"
#include "../util/lane_utils.h"
#include <raymath.h>

namespace {

// Tick all jumping entities: advance the parabolic-arc timer, update
// y_offset, and remove the Jumping component when the jump lands
// (firing the JumpLand haptic). Per Fabian, this is the "Jumping"
// table's per-row transform; entities without a Jumping row are not
// touched.
void tick_jumping(entt::registry& reg, float dt) {
    auto view = reg.view<PlayerTag, Jumping>();
    for (auto [entity, jump] : view.each()) {
        jump.timer -= dt;
        const float half = constants::JUMP_DURATION / 2.0f;
        const float t = constants::JUMP_DURATION - jump.timer;
        const float normalized = t / half - 1.0f;
        jump.y_offset = -constants::JUMP_HEIGHT * (1.0f - normalized * normalized);

        if (jump.timer <= 0.0f) {
            if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
                disp->enqueue<PlayHapticEvent>({HapticEvent::JumpLand});
            }
            reg.remove<Jumping>(entity);
        }
    }
}

// Tick all sliding entities: advance the slide timer and remove the
// Sliding component when the slide completes. Slide has no vertical
// offset — the visual squash is applied by the render system.
void tick_sliding(entt::registry& reg, float dt) {
    auto view = reg.view<PlayerTag, Sliding>();
    for (auto [entity, slide] : view.each()) {
        slide.timer -= dt;
        if (slide.timer <= 0.0f) {
            reg.remove<Sliding>(entity);
        }
    }
}

}  // namespace

void player_movement_system(entt::registry& reg, float dt) {
    auto* song = reg.ctx().find<SongState>();
    const bool rhythm_mode = (song != nullptr && (song->playing || song->finished));

    auto view = reg.view<PlayerTag, WorldPosition, PlayerShape, Lane>();
    for (auto [entity, transform, pshape, lane] : view.each()) {
        lane_utils::normalize(lane, &transform);

        // Morph animation — freeplay only.
        // In rhythm mode shape_window_system owns morph_t (derives from song_time);
        // writing it here too would double-update it every tick. (#207)
        if (!rhythm_mode && pshape.morph_t < 1.0f) {
            pshape.morph_t = Clamp(pshape.morph_t + dt / constants::MORPH_DURATION, 0.0f, 1.0f);
        }

        // Lane transition
        if (lane.target == lane.current) {
            lane.target = lane_utils::kNoTargetLane;
            lane.lerp_t = 1.0f;
            transform.position.x = constants::LANE_X[lane.current];
        }
        if (lane_utils::is_valid(lane.target) && lane.target != lane.current) {
            lane.lerp_t += dt * constants::LANE_SWITCH_SPEED;
            float from_x = constants::LANE_X[lane.current];
            float to_x   = constants::LANE_X[lane.target];
            transform.position.x = Lerp(from_x, to_x, lane.lerp_t);

            if (lane.lerp_t >= 1.0f) {
                lane.current = lane.target;
                lane.target  = lane_utils::kNoTargetLane;
                lane.lerp_t  = 1.0f;
                transform.position.x = constants::LANE_X[lane.current];
            }
        }
        (void)entity;
    }

    // Per-state vertical-motion transforms (issue #1202/#1204).
    // Grounded entities have neither Jumping nor Sliding, so no transform
    // runs for them.
    tick_jumping(reg, dt);
    tick_sliding(reg, dt);
}
