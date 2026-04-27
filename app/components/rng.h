#pragma once

#include <random>

struct RNGState {
    std::mt19937 engine{1u};
};
