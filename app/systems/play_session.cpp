#include "play_session.h"
#include "../components/game_state.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/burnout.h"
#include "../components/difficulty.h"
#include "../components/audio.h"
#include "../components/rhythm.h"
#include "../constants.h"

void setup_play_session(entt::registry& reg) {
    reg.clear();

    // Reset singletons
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

    // Create player entity (starts as Hexagon in rhythm mode)
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<Position>(player, constants::LANE_X[1], constants::PLAYER_Y);
    {
        PlayerShape ps;
        ps.current  = Shape::Hexagon;
        ps.previous = Shape::Hexagon;
        reg.emplace<PlayerShape>(player, ps);
    }
    {
        ShapeWindow sw;
        sw.target_shape = Shape::Hexagon;
        sw.phase_raw    = static_cast<uint8_t>(WindowPhase::Idle);
        reg.emplace<ShapeWindow>(player, sw);
    }
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<DrawColor>(player, uint8_t{80}, uint8_t{180}, uint8_t{255}, uint8_t{255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);

    // Reset rhythm singletons
    auto* song = reg.ctx().find<SongState>();
    if (song) {
        song->song_time      = 0.0f;
        song->current_beat   = -1;
        song->playing        = true;
        song->finished       = false;
        song->next_spawn_idx = 0;
        song->restart_music  = true;
    }
    auto* hp = reg.ctx().find<HPState>();
    if (hp) { hp->current = hp->max_hp; }
    auto* results = reg.ctx().find<SongResults>();
    if (results) { *results = SongResults{}; }

    // Transition game state
    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 0.0f;
}
