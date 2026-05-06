#pragma once

#include <entt/entt.hpp>

#include <type_traits>
#include <utility>

template <typename T, typename... Args>
T& registry_ctx_get_or_emplace(entt::registry& reg, Args&&... args) {
    if (auto* value = reg.ctx().find<T>()) {
        return *value;
    }
    return reg.ctx().emplace<T>(std::forward<Args>(args)...);
}

template <typename T, typename Registry>
decltype(auto) registry_ctx_get(Registry& reg) {
    return reg.ctx().template get<T>();
}

template <typename T, typename Registry>
auto* registry_ctx_find(Registry& reg) {
    return reg.ctx().template find<T>();
}

template <typename T>
std::decay_t<T>& registry_ctx_insert_or_assign(entt::registry& reg, T&& value) {
    using Value = std::decay_t<T>;
    return reg.ctx().insert_or_assign<Value>(std::forward<T>(value));
}

template <typename... Ts>
void registry_ctx_insert_defaults(entt::registry& reg) {
    (registry_ctx_insert_or_assign(reg, std::decay_t<Ts>{}), ...);
}

template <typename T, typename Registry, typename Fn>
void registry_ctx_if(Registry& reg, Fn&& fn) {
    if (auto* value = registry_ctx_find<T>(reg)) {
        std::forward<Fn>(fn)(*value);
    }
}
