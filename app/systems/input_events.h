#pragma once

#include <cstdint>

// ── Semantic Press / Directional Events ────────────────────────────────────
//
// Per Fabian's existential processing (issues #1202/#1204/#1277/#1279), the
// former `ButtonPressKind` / `MenuActionKind` / `Direction` discriminators
// and the union-like `MenuPressEvent::action` / `GoEvent::dir` columns were
// eradicated. Each former (kind × shape) / (action) / (direction)
// combination is now its own event type. Listeners subscribe to the specific
// event they handle, so there is no `switch (kind)` / `switch (action)` /
// `switch (dir)` and no `if (evt.kind == Foo::Bar)` at any consumer call
// site — the type IS the choice.
//
// Shape presses are zero-column tag-events: "which shape?" is identity-
// encoded in the event type itself, no payload, no runtime discriminator.
// Menu presses follow the same pattern: per-action event types, with the
// `index` payload kept only where a semantic index is actually carried
// (`MenuSelectLevelEvent`, `MenuSelectDiffEvent`). Directional intent uses
// the same identity-encoded pattern: one event type per cardinal direction.
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

// Directional intent — one zero-column event type per cardinal direction
// (issue #1279). The former `Direction` enum + `GoEvent { Direction dir }`
// were eradicated to remove the if-ladder/switch-on-discriminator pattern
// at consumer call sites. Producers emit the matching per-direction event
// directly (input_system swipe / keyboard, test_player_system auto-input);
// consumers subscribe to the specific direction(s) they handle.
struct GoUpEvent    {};
struct GoDownEvent  {};
struct GoLeftEvent  {};
struct GoRightEvent {};
