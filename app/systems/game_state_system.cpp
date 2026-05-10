#include "all_systems.h"
#include "game_phase_transition.h"
#include "../session/play_session.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../components/obstacle.h"
#include "../components/input_events.h"
#include "../components/player.h"
#include "../components/rhythm.h"
#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
#include <emscripten/emscripten.h>
#include <string>
#endif

#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
namespace {

void update_web_playing_lane_marker(entt::registry& reg, const GameState& gs) {
    if (gs.phase != GamePhase::Playing) {
        return;
    }

    auto player_view = reg.view<PlayerTag, Lane>();
    if (player_view.begin() == player_view.end()) {
        return;
    }

    const auto player_entity = *player_view.begin();
    const int lane = static_cast<int>(player_view.get<Lane>(player_entity).current);
    static int last_lane = -1;
    if (lane == last_lane) {
        return;
    }
    last_lane = lane;

    const std::string title = std::string("SHAPESHIFTER [Playing][Lane:")
        + std::to_string(lane) + "]";
    EM_ASM(
        {
            if (typeof document !== 'undefined') {
                document.title = UTF8ToString($0);
            }
        },
        title.c_str());
}

}  // namespace
#endif

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
    // Events are enqueued earlier in the same frame by input_system
    // (keyboard/swipes) and raygui HUD controllers (buttons) before the
    // fixed-step loop.
    //
    // ⚠ Do NOT call disp.clear<GoEvent/ButtonPressEvent>() before this point
    //   within a frame.  Those pre-tick systems enqueue same-frame events that
    //   must reach player_input_handle_go / player_input_handle_press through
    //   this update() call.  clear() would silently drop them before delivery.
    //
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.update<GoEvent>();
    disp.update<ButtonPressEvent>();

    if (gs.transition_pending) {
        gs.transition_pending = false;

        // Clear any in-flight pointer capture when changing screens so
        // down/up state from the previous phase cannot leak into the next UI.
        if (auto* input = reg.ctx().find<InputState>()) {
            input->touch_down = false;
            input->touch_up = false;
            input->click = false;
            input->touching = false;
            input->active_source = InputSource::None;
            input->suppress_mouse_release = false;
            input->duration = 0.0f;
        }

        switch (gs.next_phase) {
            case GamePhase::Playing:
                // Resume from Paused must NOT re-init the play session — that
                // would discard score/energy/obstacles. All other paths into
                // Playing (Title/LevelSelect/etc.) bootstrap a fresh session.
                // See #482 for the rationale that converged screen controllers
                // and input routing on the deferred (transition_pending) path.
                if (gs.phase == GamePhase::Paused) {
                    enter_phase(gs, GamePhase::Playing);
                } else {
                    setup_play_session(reg);
                }
                break;
            case GamePhase::GameOver:
                game_state_enter_terminal_phase(reg, GamePhase::GameOver);
                break;
            case GamePhase::SongComplete:
                game_state_enter_terminal_phase(reg, GamePhase::SongComplete);
                break;
            case GamePhase::Paused:
                enter_phase(gs, GamePhase::Paused);
                break;
            case GamePhase::Title:
                enter_phase(gs, GamePhase::Title);
                break;
            case GamePhase::LevelSelect:
                enter_phase(gs, GamePhase::LevelSelect);
                {
                    auto& lss = reg.ctx().get<LevelSelectState>();
                    lss.confirmed = false;
                }
                break;
            case GamePhase::Settings:
                enter_phase(gs, GamePhase::Settings);
                break;
            case GamePhase::Tutorial:
                enter_phase(gs, GamePhase::Tutorial);
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
#if defined(__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)
        update_web_playing_lane_marker(reg, gs);
#endif
        auto* energy = reg.ctx().find<EnergyState>();
        auto* song = reg.ctx().find<SongState>();
        if (energy && song && song->playing && energy->energy <= 0.0f) {
            auto* gos = reg.ctx().find<GameOverState>();
            if (gos) {
                gos->cause = DeathCause::EnergyDepleted;
            }
            gs.transition_pending = true;
            gs.next_phase = GamePhase::GameOver;
            return;
        }

        if (song && song->finished) {
            // Wait until all obstacle entities have been destroyed.
            if (reg.view<ObstacleTag>().empty()) {
                gs.transition_pending = true;
                gs.next_phase = GamePhase::SongComplete;
            }
        }
    }

    game_state_end_screen_system(reg, dt);
}
