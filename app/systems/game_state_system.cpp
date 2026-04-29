#include "all_systems.h"
#include "../session/play_session.h"
#include "../components/game_state.h"
#include "../util/obstacle_counter.h"
#include "../components/input_events.h"
#include "../components/rhythm.h"

void game_state_system(entt::registry& reg, float dt) {
    auto& gs = reg.ctx().get<GameState>();

    gs.phase_timer += dt;

    // ── Primary event drain ───────────────────────────────────────────────────
    // game_state_system runs first in tick_fixed_systems (game_loop.cpp) and
    // owns the authoritative drain for GoEvent and ButtonPressEvent.
    // Calling update<T>() here fires every registered listener in registration
    // order: game_state → level_select → player_input
    // (see wire_input_dispatcher in input/input_dispatcher.cpp).
    //
    // Events are enqueued earlier in the same frame by input_system (keyboard),
    // gesture_routing (swipes), and hit_test (taps) — all of
    // which run as direct pre-tick calls before the fixed-step loop.
    //
    // ⚠ Do NOT call disp.clear<GoEvent/ButtonPressEvent>() before this point
    //   within a frame.  Those pre-tick systems enqueue same-frame events that
    //   must reach player_input_handle_go / player_input_handle_press through
    //   this update() call.  clear() would silently drop them before delivery.
    //
    // player_input_system also calls update<GoEvent/ButtonPressEvent>() later
    // in the same tick, but the queue is already empty after this drain, so
    // those calls are no-ops — preserving the #213 no-replay invariant.
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.update<GoEvent>();
    disp.update<ButtonPressEvent>();

    if (gs.transition_pending) {
        gs.transition_pending = false;
        switch (gs.next_phase) {
            case GamePhase::Playing:      setup_play_session(reg);  break;
            case GamePhase::GameOver:
                game_state_enter_terminal_phase(reg, GamePhase::GameOver);
                break;
            case GamePhase::SongComplete:
                game_state_enter_terminal_phase(reg, GamePhase::SongComplete);
                break;
            case GamePhase::Paused:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Paused;
                gs.phase_timer = 0.0f;
                break;
            case GamePhase::Title:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Title;
                gs.phase_timer = 0.0f;
                break;
            case GamePhase::LevelSelect:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::LevelSelect;
                gs.phase_timer = 0.0f;
                {
                    auto& lss = reg.ctx().get<LevelSelectState>();
                    lss.confirmed = false;
                }
                break;
            case GamePhase::Settings:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Settings;
                gs.phase_timer = 0.0f;
                break;
            case GamePhase::Tutorial:
                gs.previous_phase = gs.phase;
                gs.phase = GamePhase::Tutorial;
                gs.phase_timer = 0.0f;
                break;
        }
        return;
    }

    // LevelSelect input handling
    if (gs.phase == GamePhase::LevelSelect && gs.phase_timer > 0.2f) {
        auto& lss = reg.ctx().get<LevelSelectState>();
        if (lss.confirmed) {
            lss.confirmed = false;
            gs.transition_pending = true;
            gs.next_phase = GamePhase::Playing;
        }
    }

    // Playing → SongComplete when song finishes (all obstacles cleared)
    if (gs.phase == GamePhase::Playing) {
        auto* energy = reg.ctx().find<EnergyState>();
        auto* song = reg.ctx().find<SongState>();
        if (energy && song && song->playing && energy->energy <= 0.0f) {
            if (auto* gos = reg.ctx().find<GameOverState>()) {
                if (gos->cause == DeathCause::None) {
                    gos->cause = DeathCause::EnergyDepleted;
                }
            }
            gs.transition_pending = true;
            gs.next_phase = GamePhase::GameOver;
            return;
        }

        if (song && song->finished) {
            // Wait until all obstacle entities have been destroyed (O(1) counter).
            auto* oc = reg.ctx().find<ObstacleCounter>();
            if (!oc || oc->count == 0) {
                gs.transition_pending = true;
                gs.next_phase = GamePhase::SongComplete;
            }
        }
    }

    game_state_end_screen_system(reg, dt);
}
