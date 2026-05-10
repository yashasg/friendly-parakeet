#include "player_entity.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
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
    {
        PlayerShape ps;
        ps.current  = Shape::Hexagon;
        ps.previous = Shape::Hexagon;
        reg.emplace<PlayerShape>(player, ps);
    }
    {
        ShapeWindow sw;
        sw.target_shape = Shape::Hexagon;
        sw.phase        = WindowPhase::Idle;
        reg.emplace<ShapeWindow>(player, sw);
    }
    reg.emplace<Lane>(player);
    reg.emplace<VerticalState>(player);
    reg.emplace<Color>(player, Color{80, 180, 255, 255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);
    reg.emplace<TagWorldPass>(player);
    return player;
}
