#pragma once

#include <entt/entt.hpp>

// Initialize title layout adapter (call once)
void title_adapter_init();

// Render title screen using generated layout (call every frame)
void title_adapter_render(const entt::registry& reg);
