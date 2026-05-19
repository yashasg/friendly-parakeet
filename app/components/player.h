#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "constants.h"
#include "tags/tags.h"

enum class Shape : uint8_t {
    Circle,
    Square,
    Triangle,
    Hexagon,
};

// Stringification for trace/log lines. Replaces the former
// `magic_enum::enum_name(Shape)` lookup used by the session logger and tests.
//
// Fabian Principle 1 / issue #1309: enum-as-lookup-key into a static table.
// Row order must match the `Shape` declaration above; the `static_assert`
// pins the table size to the trailing enumerator (`Hexagon`).
constexpr std::string_view to_string(Shape shape) noexcept {
    constexpr std::array<std::string_view, 4> kNameByShape{{
        /* Circle   */ "Circle",
        /* Square   */ "Square",
        /* Triangle */ "Triangle",
        /* Hexagon  */ "Hexagon",
    }};
    static_assert(kNameByShape.size() ==
                  static_cast<std::size_t>(Shape::Hexagon) + 1,
                  "kNameByShape must cover every Shape enumerator");

    const auto idx = static_cast<std::size_t>(shape);
    if (idx >= kNameByShape.size()) return {};
    return kNameByShape[idx];
}

// Hot render data — read by game_camera_system, player_movement_system every frame.
// The player's current shape identity lives as one of `ShapeCircleTag` /
// `ShapeSquareTag` / `ShapeTriangleTag` / `ShapeHexagonTag` on the player
// entity (issue #1202/#1204); see the non-tag helper `app/util/shape_tag.h`
// for dispatch and the rationale for the per-tag table mechanic.
struct PlayerShape {
    float morph_t  = 1.0f;
};

// Rhythm-mode shape window timing — read by shape_window_system,
// collision_system, and player input handlers. The window's phase
// (Idle/MorphIn/Active/MorphOut) lives as a per-phase tag on the player
// entity (`ShapeWindowMorphInTag` / `ShapeWindowActiveTag` /
// `ShapeWindowMorphOutTag` in `tags/tags.h`); Idle = absence of all three.
// The target shape the press is morphing toward lives as one of
// `TargetShapeCircleTag` / `TargetShapeSquareTag` / `TargetShapeTriangleTag` /
// `TargetShapeHexagonTag` on the same entity (issue #1202/#1204).
//
// Fabian Principle 3 (issue #1533): every column is always meaningful while
// `ShapeWindow` exists. The former `press_time = -1.0f` sentinel migrated to
// the `Pressed` row table (presence == "the window has a recorded press");
// the former `bool graded` migrated to the `WindowGraded` zero-column tag
// in `app/tags/tags.h` (presence == "this press has been graded").
struct ShapeWindow {
    float       window_timer  = 0.0f;
    float       window_start  = 0.0f;
};

// Recorded press for the current shape window. Presence on the player entity
// IS "the window has a press"; the `press_time` column is always meaningful
// while the row exists (Fabian Principle 3 / issue #1533). Writers:
// `player_input_system` emplaces on every shape press;
// `shape_window_system` removes on MorphOut → Idle. Readers: `collision_system`
// joins on `Pressed` to gate shape-timing grading and read the press time.
struct Pressed {
    float press_time = 0.0f;
};

// Lane occupancy. The transition columns (`target`, `lerp_t`) moved to the
// `LaneTransition` row table below per Fabian Principle 3 / issue #1533:
// every column here is always meaningful. Hot: read by `collision_system`,
// `player_input_system`, `player_movement_system`, `test_player_system`.
struct Lane {
    int8_t current = 1;
};

// In-flight lane transition. Presence on the player entity IS "the player
// is mid-lane-transition"; both columns are always meaningful while the
// row exists (Fabian Principle 3 / issue #1533). Replaces the former
// `Lane::target = -1` sentinel + always-present `Lane::lerp_t` optional.
//
// Writers: `player_input_system` emplaces on swipe (target = current ± 1,
// lerp_t = 0.0); `player_movement_system` removes when `lerp_t >= 1.0` and
// commits the new `Lane::current`.
// Readers: `player_movement_system` advances `lerp_t` and interpolates the
// world x-position; `test_player_system` reads `target` to compute the
// player's effective lane during action planning.
struct LaneTransition {
    LaneTransition() = delete;
    explicit LaneTransition(int8_t target_lane, float transition_t = 0.0f)
        : target(target_lane), lerp_t(transition_t) {
        assert(constants::is_valid_lane(target_lane));
    }

    int8_t target;
    float  lerp_t;
};

// ── Vertical motion state (per-state component tables) ──────
// Per Fabian's existential processing (issue #1202/#1204), each former
// VMode value becomes its own component table on the player entity:
//   - `Jumping` present  → entity is mid-jump  (timer + parabolic y_offset)
//   - `Sliding` present  → entity is mid-slide (timer; y_offset always 0,
//     visual squash is handled in the render system)
//   - Absence of both    → grounded (default state — no data needed)
//
// State transitions are table operations:
//   start jump:  reg.emplace<Jumping>(e, {JUMP_DURATION, 0.0f});
//   end jump:    reg.remove<Jumping>(e);
// `player_movement_system` splits per-state ticks (jumping / sliding)
// into separate transforms operating on their own filtered views.
struct Jumping {
    float timer    = 0.0f;
    float y_offset = 0.0f;
};

struct Sliding {
    float timer = 0.0f;
};
