#pragma once

namespace platform::timing {

[[nodiscard]] double now_seconds() noexcept;
void set_now_seconds_override(double seconds) noexcept;
void clear_now_seconds_override() noexcept;

}  // namespace platform::timing
