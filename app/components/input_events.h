#pragma once

#include "input.h"       // Direction
#include "game_state.h"  // GamePhase
#include "player.h"      // Shape
#include <cstdint>

// ── Raw Input Events (produced by input_system) ──────────────────────

enum class InputType : uint8_t { Tap, Swipe };

struct InputEvent {
    InputType type   = InputType::Tap;
    Direction dir    = Direction::Up;   // only meaningful for Swipe
    float     x      = 0.0f;           // virtual-space coordinates
    float     y      = 0.0f;
};

// ── UI Button Tags and Data ─────────────────────────────────────────

struct ShapeButtonTag {};

struct ShapeButtonData {
    Shape shape = Shape::Circle;
};

struct MenuButtonTag {};

enum class MenuActionKind : uint8_t {
    Confirm       = 0,
    Restart       = 1,
    GoLevelSelect = 2,
    GoMainMenu    = 3,
    Exit          = 4,
    SelectLevel   = 5,
    SelectDiff    = 6,
};

struct MenuAction {
    MenuActionKind kind  = MenuActionKind::Confirm;
    uint8_t        index = 0;   // for SelectLevel / SelectDiff
};

// ── Semantic Events (produced by hit-test helpers or keyboard) ────────
//
// ButtonPressEvent carries semantic value data encoded at hit-test time (#273).
// Consumers act on kind/shape/menu_action — never on a live entity handle,
// which would be a lifetime hazard if the button entity were destroyed between
// event production and consumption.

enum class ButtonPressKind : uint8_t {
    Shape,  // shape button pressed — use .shape
    Menu,   // menu button pressed  — use .menu_action / .menu_index
};

struct ButtonPressEvent {
    ButtonPressKind kind        = ButtonPressKind::Shape;
    Shape           shape       = Shape::Circle;           // valid when kind == Shape
    MenuActionKind  menu_action = MenuActionKind::Confirm; // valid when kind == Menu
    uint8_t         menu_index  = 0;                       // valid when kind == Menu
};

struct GoEvent {
    Direction dir = Direction::Up;
};

// ── Hit-Test Components (on UI button entities) ──────────────────────

struct HitBox {
    float half_w = 0.0f;
    float half_h = 0.0f;
};

struct HitCircle {
    float radius = 0.0f;
};

struct ActiveInPhase {
    GamePhaseBit phase_mask = GamePhaseBit{};
};

// Zero-size structural tag. Present iff the entity's ActiveInPhase mask covers
// the current GamePhase. Maintained by input routing helpers; consumers
// (hit-test helpers) iterate view<..., ActiveTag>() without any runtime
// predicate, so the per-event hot path is O(active buttons) instead of
// O(all buttons with ActiveInPhase).
struct ActiveTag {};

// Cache of the last phase ActiveTag was synced for. Used to skip the sync
// pass when the phase has not changed since the previous call.
struct UIActiveCache {
    GamePhase phase = GamePhase::Title;
    bool      valid = false;
};
