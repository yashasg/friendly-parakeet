#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/input.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/particle.h"
#include "../components/scoring.h"
#include "../components/burnout.h"
#include "../components/difficulty.h"
#include "../components/audio.h"
#include "../components/rhythm.h"
#include "../constants.h"

static void enter_playing(entt::registry& reg) {
    // Destroy all existing entities
    reg.clear();

    // Re-emplace singletons
    reg.ctx().insert_or_assign(ScoreState{});
    reg.ctx().insert_or_assign(DifficultyConfig{
        .speed_multiplier     = 1.0f,
        .scroll_speed         = constants::BASE_SCROLL_SPEED,
        .spawn_interval       = constants::INITIAL_SPAWN_INT,
        .spawn_timer          = 1.0f,
        .burnout_window_scale = 1.0f,
        .elapsed              = 0.0f
    });
    reg.ctx().insert_or_assign(BurnoutState{});
    reg.ctx().insert_or_assign(AudioQueue{});

    // Create player (starts as Hexagon in rhythm mode)
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<Position>(player, constants::LANE_X[1], constants::PLAYER_Y);
    {
        PlayerShape ps;
        ps.current      = Shape::Hexagon;
        ps.previous     = Shape::Hexagon;
        ps.target_shape = Shape::Hexagon;
        ps.phase_raw    = static_cast<uint8_t>(WindowPhase::Idle);
        reg.emplace<PlayerShape>(player, ps);
    }
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<DrawColor>(player, uint8_t{80}, uint8_t{180}, uint8_t{255}, uint8_t{255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);

    // Reset rhythm singletons for new play session
    auto* song = reg.ctx().find<SongState>();
    if (song) {
        song->song_time      = 0.0f;
        song->current_beat   = -1;
        song->playing        = true;
        song->finished       = false;
        song->next_spawn_idx = 0;
    }
    auto* hp = reg.ctx().find<HPState>();
    if (hp) { hp->current = hp->max_hp; }
    auto* results = reg.ctx().find<SongResults>();
    if (results) { *results = SongResults{}; }

    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 0.0f;
}

static void enter_game_over(entt::registry& reg) {
    auto& score = reg.ctx().get<ScoreState>();
    if (score.score > score.high_score) {
        score.high_score = score.score;
    }
    audio_push(reg.ctx().get<AudioQueue>(), SFX::Crash);

    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;
}

void game_state_system(entt::registry& reg, float dt) {
    auto& gs    = reg.ctx().get<GameState>();
    auto& input = reg.ctx().get<InputState>();

    gs.phase_timer += dt;

    if (gs.transition_pending) {
        gs.transition_pending = false;
        switch (gs.next_phase) {
            case GamePhase::Playing:  enter_playing(reg);  break;
            case GamePhase::GameOver: enter_game_over(reg); break;
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
        }
        return;
    }

    // Title → Playing on any touch
    if (gs.phase == GamePhase::Title && input.touch_up) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }

    // GameOver → Playing on any touch (after brief delay)
    if (gs.phase == GamePhase::GameOver && input.touch_up && gs.phase_timer > 0.4f) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }

    // Paused → resume on touch
    if (gs.phase == GamePhase::Paused && input.touch_up) {
        gs.previous_phase = gs.phase;
        gs.phase = GamePhase::Playing;
        gs.phase_timer = 0.0f;
    }
}
