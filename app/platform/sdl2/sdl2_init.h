#pragma once

namespace platform::sdl2 {

bool initialize();
void shutdown();
[[nodiscard]] const char* last_error() noexcept;
[[nodiscard]] bool is_initialized() noexcept;

}  // namespace platform::sdl2
