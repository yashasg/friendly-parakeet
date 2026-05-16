#pragma once

#include <cstdint>

// ── Directions ──────────────────────────────────────────────────────────────
// Directional intent shared by gesture producers (input_system,
// test_player_system) and listeners (player_input_system, level_select
// controller). Lives next to `GoEvent` — its only carrier (issue #1194).

enum class Direction : uint8_t { Left, Right, Up, Down };

enum class MenuActionKind : uint8_t {
    Confirm       = 0,
    Restart       = 1,
    GoLevelSelect = 2,
    GoMainMenu    = 3,
    Exit          = 4,
    SelectLevel   = 5,
    SelectDiff    = 6,
};

// ── Semantic Press Events (produced by HUD controllers, test player, or keyboard) ──
//
// Per Fabian's existential processing (issues #1202/#1204), the former
// `ButtonPressKind` discriminator and the union-like `ButtonPressEvent::shape`
// column were eradicated. Each former (kind × shape) combination is now its
// own event type. Listeners subscribe to the specific event they handle, so
// there is no `switch (kind)` and no `if (evt.kind == Foo::Bar)` at any
// consumer call site.
//
// Shape presses are zero-column tag-events: "which shape?" is identity-
// encoded in the event type itself, no payload, no runtime discriminator.
// Menu presses keep the two semantic columns (action + index); per-action
// dispatch lives in the menu consumers, still keyed on the `MenuActionKind`
// label (allowlisted as a Keep enum: input event label).
//
// Producers never store an entity handle in any event — that was the
// pre-#273 lifetime hazard and stays out of the new design.
struct ShapePressCircleEvent   {};
struct ShapePressSquareEvent   {};
struct ShapePressTriangleEvent {};

struct MenuPressEvent {
    MenuActionKind action = MenuActionKind::Confirm;
    uint8_t        index  = 0;
};

struct GoEvent {
    Direction dir = Direction::Up;
};
