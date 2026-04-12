#pragma once

// Unified platform capability flag for keyboard input.
// Defined for desktop (Windows/macOS/Linux) and web (Emscripten) builds.
// Mobile (Android/iOS) gets no keyboard handling compiled in.
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
#define PLATFORM_HAS_KEYBOARD 1
#endif
