#pragma once

#include "../components/player.h"
#include <optional>

[[nodiscard]] std::optional<Shape> shape_button_hit_test(float x, float y);
