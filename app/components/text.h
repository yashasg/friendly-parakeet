#pragma once

#include <cstdint>

// UI/domain-facing text enums used by ECS components.
// Runtime font resources are owned by TextContext in app/rendering/text_resources.h.

enum class TextAlign : uint8_t { Left, Center, Right };

// Named constants for the pre-loaded font sizes.
enum class FontSize : int { Small = 0, Medium = 1, Large = 2 };
