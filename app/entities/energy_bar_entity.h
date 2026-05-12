#pragma once

#include <entt/entt.hpp>

// Creates the singleton HUD energy bar entity and returns its handle.
// Throws std::logic_error if the registry already has an EnergyBarTag entity.
entt::entity create_energy_bar_entity(entt::registry& reg);
