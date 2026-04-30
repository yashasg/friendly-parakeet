#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/rhythm.h"
#include "../components/transform.h"
#include "../components/haptics.h"
#include "../util/haptic_queue.h"
#include "../util/settings.h"
#include "../constants.h"
#include <raymath.h>

void player_movement_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();
    bool rhythm_mode = (song != nullptr && song->playing);

    auto view = reg.view<PlayerTag, WorldTransform, PlayerShape, Lane, VerticalState>();
    for (auto [entity, transform, pshape, lane, vstate] : view.each()) {

        // Morph animation — freeplay only.
        // In rhythm mode shape_window_system owns morph_t (derives from song_time);
        // writing it here too would double-update it every tick. (#207)
        if (!rhythm_mode && pshape.morph_t < 1.0f) {
            pshape.morph_t = Clamp(pshape.morph_t + dt / constants::MORPH_DURATION, 0.0f, 1.0f);
        }

        // Lane transition
        if (lane.target >= 0 && lane.target != lane.current) {
            lane.lerp_t += dt * constants::LANE_SWITCH_SPEED;
            float from_x = constants::LANE_X[lane.current];
            float to_x   = constants::LANE_X[lane.target];
            transform.position.x = Lerp(from_x, to_x, lane.lerp_t);

            if (lane.lerp_t >= 1.0f) {
                lane.current = lane.target;
                lane.target  = -1;
                lane.lerp_t  = 1.0f;
                transform.position.x = constants::LANE_X[lane.current];
            }
        }

        if (auto* pos = reg.try_get<Position>(entity)) {
            pos->x = transform.position.x;
            pos->y = transform.position.y;
        }

        // Vertical movement
        if (vstate.mode != VMode::Grounded) {
            vstate.timer -= dt;

            if (vstate.mode == VMode::Jumping) {
                // Parabolic jump: peak at half duration
                float half = constants::JUMP_DURATION / 2.0f;
                float t = constants::JUMP_DURATION - vstate.timer;
                float normalized = t / half - 1.0f;
                vstate.y_offset = -constants::JUMP_HEIGHT * (1.0f - normalized * normalized);
            } else if (vstate.mode == VMode::Sliding) {
                vstate.y_offset = 0.0f; // visual handled in render (squash)
            }

            if (vstate.timer <= 0.0f) {
                const bool was_jumping = (vstate.mode == VMode::Jumping);
                if (was_jumping) {
                    // Haptic on landing (spec: "Jump (land)" not takeoff)
                    auto* hq = reg.ctx().find<HapticQueue>();
                    auto* st = reg.ctx().find<SettingsState>();
                    if (hq) haptic_push(*hq, !st || st->haptics_enabled, HapticEvent::JumpLand);
                }
                vstate.mode     = VMode::Grounded;
                vstate.timer    = 0.0f;
                vstate.y_offset = 0.0f;
            }
        }
    }
}
