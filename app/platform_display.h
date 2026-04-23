#pragma once
#include <entt/entt.hpp>
#include <raylib.h>

// Platform-specific display functions.
// Native and Emscripten provide different implementations.

// Read the current window/canvas size in logical pixels.
void platform_get_display_size(float& out_w, float& out_h);

// Prepare the framebuffer for blitting (Emscripten needs viewport fixup).
void platform_pre_blit();

// Start the platform main loop. On Emscripten this never returns.
// On native this is a no-op — the caller runs its own while loop.
#ifdef __EMSCRIPTEN__
void platform_run_loop(entt::registry& reg, RenderTexture2D& target);
#endif
