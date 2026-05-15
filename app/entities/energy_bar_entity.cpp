#include "energy_bar_entity.h"

#include "../components/energy_bar.h"
#include "../components/rendering.h"

#include <stdexcept>

entt::entity create_energy_bar_entity(entt::registry& reg) {
    auto existing = reg.view<EnergyBarTag>();
    if (existing.begin() != existing.end()) {
        throw std::logic_error("EnergyBarTag entity already exists");
    }

    auto energy_bar = reg.create();
    reg.emplace<EnergyBarTag>(energy_bar);
    reg.emplace<EnergyBarLayout>(energy_bar);
    reg.emplace<EnergyBarVisual>(energy_bar);
    reg.emplace<DrawLayer>(energy_bar, Layer::HUD);
    reg.emplace<TagHUDPass>(energy_bar);
    return energy_bar;
}
