#pragma once

#include <entt/entt.hpp>
#include <glm/vec2.hpp>
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
};

struct ObstacleDespawnScratch {
    std::vector<entt::entity> to_destroy;
};

struct PopupDisplayScratch {
    std::vector<entt::entity> expired;
};

struct ParticleSystemScratch {
    std::vector<entt::entity> expired;
};
