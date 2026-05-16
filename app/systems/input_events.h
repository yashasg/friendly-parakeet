#pragma once

#include <cstdint>

// ── Directions ──────────────────────────────────────────────────────────────
// Directional intent shared by gesture producers (input_system,
// test_player_system) and listeners (player_input_system, level_select
// controller). Lives next to `GoEvent` — its only carrier (issue #1194).

enum class Direction : uint8_t { Left, Right, Up, Down };

// ── Semantic Press Events (produced by HUD controllers, test player, or keyboard) ──
//
// Per Fabian's existential processing (issues #1202/#1204/#1277), the former
// `ButtonPressKind` and `MenuActionKind` discriminators and the union-like
// `MenuPressEvent::action` column were eradicated. Each former
// (kind × shape) and (action) combination is now its own event type.
// Listeners subscribe to the specific event they handle, so there is no
// `switch (kind)` / `switch (action)` and no `if (evt.kind == Foo::Bar)`
// at any consumer call site.
//
// Shape presses are zero-column tag-events: "which shape?" is identity-
// encoded in the event type itself, no payload, no runtime discriminator.
// Menu presses follow the same pattern: per-action event types, with the
// `index` payload kept only where a semantic index is actually carried
// (`MenuSelectLevelEvent`, `MenuSelectDiffEvent`).
//
// Producers never store an entity handle in any event — that was the
// pre-#273 lifetime hazard and stays out of the new design.
struct ShapePressCircleEvent   {};
struct ShapePressSquareEvent   {};
struct ShapePressTriangleEvent {};

// Generic "proceed" press: Enter / Space / Title-tap / Pause-tap / FTUE-tap /
// end-screen tap. Per-phase routing lives on the listener side and dispatches
// purely on the ctx phase tag (no enum discriminator).
struct MenuConfirmEvent {};

// End-screen choice events. `MenuConfirmEvent` on an end screen is also
// routed to the "restart" path inside the listener so the keyboard Enter
// shortcut still restarts the run.
struct MenuRestartEvent       {};
struct MenuGoLevelSelectEvent {};
struct MenuGoMainMenuEvent    {};

// Level-select numeric selection events. Index is bounds-checked at the
// listener against the static content config.
struct MenuSelectLevelEvent { uint8_t index = 0; };
struct MenuSelectDiffEvent  { uint8_t index = 0; };

struct GoEvent {
    Direction dir = Direction::Up;
};
