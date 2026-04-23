#pragma once

// Axis-aligned rectangle hit-test with uniform padding, for UI button
// click resolution in input-consumer systems (title screen, level select,
// end screens). Padding expands the hit region on all four sides, making
// touch targets more forgiving without changing the visual bounds.
[[nodiscard]] inline bool point_in_padded_rect(float px, float py,
                                               float rx, float ry,
                                               float rw, float rh,
                                               float pad = 0.0f) {
    return px >= rx - pad && px <= rx + rw + pad &&
           py >= ry - pad && py <= ry + rh + pad;
}
