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

enum class MenuActionKind : uint8_t {
    Confirm       = 0,
    Restart       = 1,
    GoLevelSelect = 2,
    GoMainMenu    = 3,
    Exit          = 4,
    SelectLevel   = 5,
    SelectDiff    = 6,
};

// ── Semantic Events (produced by HUD controllers, test player, or keyboard) ────────
//
// ButtonPressEvent carries semantic value data encoded at source (#273).
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
