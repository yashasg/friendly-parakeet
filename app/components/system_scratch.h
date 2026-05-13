#pragma once

#include <entt/entt.hpp>
#include <glm/vec2.hpp>
#include <cstdint>
#include <vector>

#include "obstacle.h"
#include "rhythm.h"

struct MissRecord {
    entt::entity e = entt::null;
    bool has_timing = false;
};

struct HitRecord {
    entt::entity e = entt::null;
    glm::vec2 popup_xy = {0.0f, 0.0f};
    Obstacle obs;
    bool has_timing = false;
    TimingGrade timing{};
};

struct ScoringSystemScratch {
    std::vector<MissRecord> miss_buf;
    std::vector<HitRecord> hit_buf;
    uint32_t miss_capacity_exceeded_count = 0;
    uint32_t hit_capacity_exceeded_count = 0;
};

struct ObstacleDespawnScratch {
    std::vector<entt::entity> to_destroy;
    uint32_t capacity_exceeded_count = 0;
};

struct PopupDisplayScratch {
    std::vector<entt::entity> expired;
    uint32_t capacity_exceeded_count = 0;
};

struct ParticleSystemScratch {
    std::vector<entt::entity> expired;
    uint32_t capacity_exceeded_count = 0;
};

struct MeshChildCleanupScratch {
    std::vector<entt::entity> stale_children;
    uint32_t capacity_exceeded_count = 0;
};

struct WasmSmokeLaneMarkerState {
    int last_lane = -1;
};
