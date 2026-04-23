#pragma once

#include <entt/entt.hpp>
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

// ── Semantic Events (produced by hit_test_system or keyboard) ────────

struct ButtonPressEvent {
    entt::entity entity = entt::null;
};

struct GoEvent {
    Direction dir = Direction::Up;
};

// ── Event Queue Singleton ────────────────────────────────────────────

struct EventQueue {
    static constexpr int MAX = 8;

    InputEvent       inputs[MAX]  = {};
    int              input_count  = 0;

    ButtonPressEvent presses[MAX] = {};
    int              press_count  = 0;

    GoEvent          goes[MAX]    = {};
    int              go_count     = 0;

    void push_input(InputType t, float px, float py,
                    Direction d = Direction::Up) {
        if (input_count < MAX) {
            inputs[input_count++] = {t, d, px, py};
        }
    }
    void push_press(entt::entity e) {
        if (press_count < MAX) {
            presses[press_count++] = {e};
        }
    }
    void push_go(Direction d) {
        if (go_count < MAX) {
            goes[go_count++] = {d};
        }
    }
    void clear() {
        input_count = 0;
        press_count = 0;
        go_count    = 0;
    }
};

// ── Hit-Test Components (on UI button entities) ──────────────────────

struct HitBox {
    float half_w = 0.0f;
    float half_h = 0.0f;
};

struct HitCircle {
    float radius = 0.0f;
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

struct ActiveInPhase {
    uint8_t phase_mask = 0;
};

// Helper: check if a GamePhase is active in the mask
inline bool phase_active(const ActiveInPhase& aip, GamePhase phase) {
    return (aip.phase_mask >> static_cast<uint8_t>(phase)) & 1;
}

// Helper: build mask from a single phase
inline uint8_t phase_bit(GamePhase p) {
    return uint8_t(1) << static_cast<uint8_t>(p);
}
