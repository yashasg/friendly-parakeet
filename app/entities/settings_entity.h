#pragma once

#include <entt/entt.hpp>

#include "../components/settings.h"

// Creates the singleton settings entity and returns its handle.
// Throws std::logic_error if the registry already has a SettingsTag entity.
entt::entity create_settings_entity(entt::registry& reg,
                                    SettingsState state = {},
                                    SettingsPersistence persistence = {});

void destroy_settings_entity(entt::registry& reg);
