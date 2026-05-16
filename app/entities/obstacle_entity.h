#pragma once

#include <entt/entt.hpp>
#include "../components/player.h"  // Shape
#include "../components/rhythm.h"  // BeatInfo

// Per-kind spawn-parameter rows (issue #1202/#1204). Each former
// `ObstacleKind` value has its own row schema:
//   - `ShapeGateSpawn`: shape (gate prompt) + positional lane
//   - `SplitPathSpawn`: shape + required dodge lane
//   - `OnsetMarkerSpawn`: position + speed only
// `req_lane` for ShapeGate is computed at spawn-time from x via
// `nearest_lane_for_x()` and stored in `ShapeGateLane`. SplitPath uses
// `req_lane` directly to set `RequiredLane`.
struct ShapeGateSpawn {
    float  x      = 0.0f;
    float  y      = 0.0f;
    Shape  shape  = Shape::Circle;
    float  speed  = 0.0f;
};

struct SplitPathSpawn {
    float  x        = 0.0f;
    float  y        = 0.0f;
    Shape  shape    = Shape::Circle;
    int8_t req_lane = 0;
    float  speed    = 0.0f;
};

struct OnsetMarkerSpawn {
    float x     = 0.0f;
    float y     = 0.0f;
    float speed = 0.0f;
};

// Per-kind obstacle spawn entry points. Each emplaces:
//   ObstacleTag + TagWorldPass + the kind's per-tag table (ShapeGateTag /
//   SplitPathTag / OnsetMarkerTag) + the per-instance data components
//   that case carries + mesh children.
// Non-rhythm variants attach a Vector2 velocity (raw type, see
// transform.h); rhythm variants attach a `BeatInfo` instead.
entt::entity spawn_shape_gate_obstacle  (entt::registry& reg, const ShapeGateSpawn& params);
entt::entity spawn_split_path_obstacle  (entt::registry& reg, const SplitPathSpawn& params);
entt::entity spawn_onset_marker_obstacle(entt::registry& reg, const OnsetMarkerSpawn& params);

entt::entity spawn_shape_gate_rhythm  (entt::registry& reg, const ShapeGateSpawn& params,
                                       const BeatInfo& beat_info);
entt::entity spawn_split_path_rhythm  (entt::registry& reg, const SplitPathSpawn& params,
                                       const BeatInfo& beat_info);
entt::entity spawn_onset_marker_rhythm(entt::registry& reg, const OnsetMarkerSpawn& params,
                                       const BeatInfo& beat_info);
