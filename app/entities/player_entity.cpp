#include "player_entity.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"
#include "../util/shape_tag.h"
#include <stdexcept>

entt::entity create_player_entity(entt::registry& reg) {
    auto existing = reg.view<PlayerTag>();
    if (existing.begin() != existing.end()) {
        throw std::logic_error("PlayerTag entity already exists");
    }

    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<WorldPosition>(player, WorldPosition{{constants::LANE_X[1], constants::PLAYER_Y}});
    reg.emplace<PlayerShape>(player);
    // Player starts as Hexagon: presence of `ShapeHexagonTag` IS the data
    // (issue #1202/#1204 — see `app/util/shape_tag.h`).
    set_player_shape_tag(reg, player, Shape::Hexagon);
    reg.emplace<ShapeWindow>(player);
    set_target_shape_tag(reg, player, Shape::Hexagon);
    // Idle phase = absence of all ShapeWindow*Tag (per #1202/#1204).
    reg.emplace<Lane>(player);
    // Vertical motion is grounded by default — no Jumping/Sliding component
    // is emplaced (Grounded == absence of both per #1202/#1204).
    reg.emplace<Color>(player, Color{80, 180, 255, 255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<TagWorldPass>(player);
    return player;
}
