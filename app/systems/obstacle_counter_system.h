#pragma once

// Registry context singleton — not an entity component.
// Maintained O(1) counter for live obstacle entities.
// Incremented via on_construct<ObstacleTag> signal.
// Decremented via on_destroy<ObstacleTag>  signal.
// game_state_system reads count == 0 to detect "all obstacles drained".
struct ObstacleCounter {
    int  count = 0;
    bool wired = false;
};
