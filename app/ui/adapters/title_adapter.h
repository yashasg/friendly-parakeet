#pragma once

#include <entt/entt.hpp>

// Initialize title layout adapter (call once)
void title_adapter_init();

// Render title screen using generated layout and dispatch button actions.
// Takes non-const registry because raygui processes input during draw,
// and button press results are dispatched directly to game state.
void title_adapter_render(entt::registry& reg);
