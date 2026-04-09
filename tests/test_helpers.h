#pragma once

#include <entt/entt.hpp>
#include "components/transform.h"
#include "components/player.h"
#include "components/obstacle.h"
#include "components/obstacle_data.h"
#include "components/input.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/burnout.h"
#include "components/difficulty.h"
#include "components/rendering.h"
#include "components/lifetime.h"
#include "components/particle.h"
#include "components/audio.h"
#include "constants.h"
#include "systems/all_systems.h"

// Sets up a registry with all singletons in their default state
inline entt::registry make_registry() {
    entt::registry reg;
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<GestureResult>();
    reg.ctx().emplace<ShapeButtonEvent>();
    reg.ctx().emplace<GameState>(GameState{
        GamePhase::Playing, GamePhase::Playing, 0.0f, false, GamePhase::Playing, 0.0f
    });
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<BurnoutState>();
    reg.ctx().emplace<DifficultyConfig>();
    reg.ctx().emplace<AudioQueue>();
    return reg;
}

// Creates a player entity in lane 1 (center) with default shape (Circle)
inline entt::entity make_player(entt::registry& reg) {
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<Position>(player, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<PlayerShape>(player);
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<Color>(player, uint8_t{80}, uint8_t{180}, uint8_t{255}, uint8_t{255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);
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
    reg.emplace<Color>(obs, uint8_t{255}, uint8_t{255}, uint8_t{255}, uint8_t{255});
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
    reg.emplace<Color>(obs, uint8_t{255}, uint8_t{60}, uint8_t{60}, uint8_t{255});
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
    reg.emplace<Color>(obs, uint8_t{255}, uint8_t{180}, uint8_t{0}, uint8_t{255});
    return obs;
}
