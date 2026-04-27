#pragma once

#include <entt/entt.hpp>
#include "components/transform.h"
#include "components/player.h"
#include "components/obstacle.h"
#include "components/obstacle_counter.h"
#include "components/obstacle_data.h"
#include "components/input.h"
#include "components/input_events.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/difficulty.h"
#include "components/rendering.h"
#include "components/lifetime.h"
#include "components/particle.h"
#include "components/audio.h"
#include "components/haptics.h"
#include "components/settings.h"
#include "components/rhythm.h"
#include "components/high_score.h"
#include "components/rng.h"
#include "constants.h"
#include "systems/all_systems.h"

// Sets up a registry with all singletons in their default state
inline entt::registry make_registry() {
    entt::registry reg;
    // Prime ObstacleChildren pool before wire_obstacle_counter creates the ObstacleTag
    // pool. EnTT::destroy() iterates pools in reverse insertion order; ObstacleChildren
    // must have a lower index so it is still accessible when on_obstacle_destroy fires.
    reg.storage<ObstacleChildren>();
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<EventQueue>();
    reg.ctx().emplace<entt::dispatcher>();
    wire_input_dispatcher(reg);
    reg.ctx().emplace<GameState>(GameState{
        GamePhase::Playing, GamePhase::Playing, 0.0f, false, GamePhase::Playing, 0.0f
    });
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<DifficultyConfig>();
    reg.ctx().emplace<AudioQueue>();
    reg.ctx().emplace<HapticQueue>();
    reg.ctx().emplace<SettingsState>();  // defaults: haptics_enabled=true
    reg.ctx().emplace<LevelSelectState>();
    reg.ctx().emplace<BeatMap>();
    reg.ctx().emplace<SongState>();
    reg.ctx().emplace<EnergyState>();
    reg.ctx().emplace<SongResults>();
    reg.ctx().emplace<HighScoreState>();
    reg.ctx().emplace<HighScorePersistence>();
    reg.ctx().emplace<GameOverState>();
    reg.ctx().emplace<RNGState>();
    reg.ctx().emplace<ObstacleCounter>();
    wire_obstacle_counter(reg);
    return reg;
}

struct GoCapture {
    GoEvent  buf[8] = {};
    int      count  = 0;
    void capture(const GoEvent& e) { if (count < 8) buf[count++] = e; }
};

struct PressCapture {
    ButtonPressEvent buf[8] = {};
    int              count  = 0;
    void capture(const ButtonPressEvent& e) { if (count < 8) buf[count++] = e; }
};

inline GoCapture drain_go_events(entt::registry& reg) {
    GoCapture cap;
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<GoEvent>().connect<&GoCapture::capture>(cap);
    disp.update<GoEvent>();
    disp.sink<GoEvent>().disconnect<&GoCapture::capture>(cap);
    return cap;
}

inline PressCapture drain_press_events(entt::registry& reg) {
    PressCapture cap;
    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<ButtonPressEvent>().connect<&PressCapture::capture>(cap);
    disp.update<ButtonPressEvent>();
    disp.sink<ButtonPressEvent>().disconnect<&PressCapture::capture>(cap);
    return cap;
}

// Sets up a registry with rhythm singletons included
inline entt::registry make_rhythm_registry() {
    entt::registry reg = make_registry();
    auto& song = reg.ctx().get<SongState>();
    song.bpm = 120.0f;
    song.offset = 0.0f;
    song.lead_beats = 4;
    song.duration_sec = 60.0f;
    song.playing = true;
    song_state_compute_derived(song);
    reg.ctx().get<EnergyState>() = EnergyState{};
    reg.ctx().get<SongResults>() = SongResults{};
    return reg;
}

// Creates a player entity in lane 1 (center) with default shape (Hexagon in rhythm mode)
inline entt::entity make_player(entt::registry& reg) {
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<Position>(player, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<PlayerShape>(player);
    reg.emplace<ShapeWindow>(player);
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<Color>(player, Color{80, 180, 255, 255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);
    return player;
}

// Creates a player entity for rhythm mode (starts as Hexagon)
inline entt::entity make_rhythm_player(entt::registry& reg) {
    auto player = make_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    ps.current = Shape::Hexagon;
    ps.previous = Shape::Hexagon;
    auto& sw = reg.get<ShapeWindow>(player);
    sw.target_shape = Shape::Hexagon;
    sw.phase = WindowPhase::Idle;
    return player;
}

// Creates a shape gate obstacle at given y, requiring given shape
inline entt::entity make_shape_gate(entt::registry& reg, Shape shape, float y) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(obs, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<RequiredShape>(obs, shape);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{255, 255, 255, 255});
    return obs;
}

// Creates a lane block obstacle blocking specified lanes (bitmask)
inline entt::entity make_lane_block(entt::registry& reg, uint8_t mask, float y) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(obs, ObstacleKind::LaneBlock, int16_t{constants::PTS_LANE_BLOCK});
    reg.emplace<BlockedLanes>(obs, mask);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W / 3), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{255, 60, 60, 255});
    return obs;
}

// Creates a low bar (must jump) or high bar (must slide) obstacle
inline entt::entity make_vertical_bar(entt::registry& reg, ObstacleKind kind, float y) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    int16_t pts = (kind == ObstacleKind::LowBar) ? constants::PTS_LOW_BAR : constants::PTS_HIGH_BAR;
    reg.emplace<Obstacle>(obs, kind, pts);
    VMode action = (kind == ObstacleKind::LowBar) ? VMode::Jumping : VMode::Sliding;
    reg.emplace<RequiredVAction>(obs, action);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 40.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{255, 180, 0, 255});
    return obs;
}

// Creates a combo gate requiring shape AND lane not blocked
inline entt::entity make_combo_gate(entt::registry& reg, Shape shape, uint8_t blocked_mask, float y) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(obs, ObstacleKind::ComboGate, int16_t{constants::PTS_COMBO_GATE});
    reg.emplace<RequiredShape>(obs, shape);
    reg.emplace<BlockedLanes>(obs, blocked_mask);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{200, 100, 255, 255});
    return obs;
}

// Creates a split path requiring shape AND specific lane
inline entt::entity make_split_path(entt::registry& reg, Shape shape, int8_t lane, float y) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(obs, ObstacleKind::SplitPath, int16_t{constants::PTS_SPLIT_PATH});
    reg.emplace<RequiredShape>(obs, shape);
    reg.emplace<RequiredLane>(obs, lane);
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{255, 215, 0, 255});
    return obs;
}

// Creates a LanePushLeft or LanePushRight obstacle (no data components)
inline entt::entity make_lane_push(entt::registry& reg, ObstacleKind kind, float y) {
    auto& config = reg.ctx().get<DifficultyConfig>();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, constants::LANE_X[1], y);
    reg.emplace<Velocity>(obs, 0.0f, config.scroll_speed);
    reg.emplace<Obstacle>(obs, kind, int16_t{0});
    reg.emplace<DrawSize>(obs, float(constants::SCREEN_W / 3), 80.0f);
    reg.emplace<DrawLayer>(obs, Layer::Game);
    reg.emplace<Color>(obs, Color{0, 200, 200, 255});
    return obs;
}

// ── UI Button helpers for tests ──────────────────────────────────────────────

inline entt::entity make_shape_button(entt::registry& reg, Shape shape) {
    auto btn = reg.create();
    reg.emplace<ShapeButtonTag>(btn);
    reg.emplace<ShapeButtonData>(btn, shape);
    reg.emplace<Position>(btn, 0.0f, 0.0f);
    reg.emplace<HitCircle>(btn, 50.0f);
    reg.emplace<ActiveInPhase>(btn, phase_bit(GamePhase::Playing));
    return btn;
}

inline entt::entity make_menu_button(entt::registry& reg, MenuActionKind kind,
                                      GamePhase phase, uint8_t index = 0) {
    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<MenuAction>(btn, kind, index);
    reg.emplace<Position>(btn, 0.0f, 0.0f);
    reg.emplace<HitBox>(btn, 100.0f, 100.0f);
    reg.emplace<ActiveInPhase>(btn, phase_bit(phase));
    return btn;
}
