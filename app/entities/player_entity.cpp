#include "player_entity.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../constants.h"
#include <stdexcept>

entt::entity create_player_entity(entt::registry& reg) {
    auto existing = reg.view<PlayerTag>();
    if (existing.begin() != existing.end()) {
        throw std::logic_error("PlayerTag entity already exists");
    }

    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<WorldTransform>(player, WorldTransform{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<PlayerShape>(player, PlayerShape{Shape::Hexagon, Shape::Hexagon, 1.0f});
    reg.emplace<ShapeWindow>(player, ShapeWindow{Shape::Hexagon, WindowPhase::Idle});
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<SDL_Color>(player, SDL_Color{80, 180, 255, 255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);
    reg.emplace<TagWorldPass>(player);
    return player;
}
