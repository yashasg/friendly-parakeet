#include "settings_entity.h"

#include <stdexcept>
#include <utility>

#include "tags/tags.h"

entt::entity create_settings_entity(entt::registry& reg,
                                    SettingsState state,
                                    SettingsPersistence persistence) {
    auto existing = reg.view<SettingsTag>();
    if (existing.begin() != existing.end()) {
        throw std::logic_error("SettingsTag entity already exists");
    }

    auto entity = reg.create();
    reg.emplace<SettingsTag>(entity);
    reg.emplace<SettingsState>(entity, state);
    reg.emplace<SettingsPersistence>(entity, std::move(persistence));
    return entity;
}

void destroy_settings_entity(entt::registry& reg) {
    auto view = reg.view<SettingsTag>();
    auto it = view.begin();
    if (it == view.end()) {
        return;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple Settings entities exist");
    }
    reg.destroy(entity);
}
