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

    void push_input(InputType t, float px, float py,
                    Direction d = Direction::Up) {
        if (input_count < MAX) {
            inputs[input_count++] = {t, d, px, py};
        }
    }
    void clear() {
        input_count = 0;
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

// Zero-size structural tag. Present iff the entity's ActiveInPhase mask covers
// the current GamePhase. Maintained by ensure_active_tags_synced(); consumers
// (hit_test_system) iterate view<..., ActiveTag>() without any runtime
// predicate, so the per-event hot path is O(active buttons) instead of
// O(all buttons with ActiveInPhase).
struct ActiveTag {};

// Cache of the last phase ActiveTag was synced for. Used to skip the sync
// pass when the phase has not changed since the previous call.
struct UIActiveCache {
    GamePhase phase = GamePhase::Title;
    bool      valid = false;
};

// Helper: check if a GamePhase is active in the mask
inline bool phase_active(const ActiveInPhase& aip, GamePhase phase) {
    return (aip.phase_mask >> static_cast<uint8_t>(phase)) & 1;
}

// Helper: build mask from a single phase
inline uint8_t phase_bit(GamePhase p) {
    return uint8_t(1) << static_cast<uint8_t>(p);
}

// Force the next ensure_active_tags_synced() call to do a full resync.
// Call after spawning/destroying buttons or mutating GameState.phase outside
// the normal phase-transition seam.
inline void invalidate_active_tag_cache(entt::registry& reg) {
    auto* cache = reg.ctx().find<UIActiveCache>();
    if (!cache) cache = &reg.ctx().emplace<UIActiveCache>();
    cache->valid = false;
}

// Sync ActiveTag presence against ActiveInPhase masks for the current
// GameState.phase. No-op when the phase has not changed since the last sync,
// so the per-frame cost in steady state is O(1).
inline void ensure_active_tags_synced(entt::registry& reg) {
    auto* cache = reg.ctx().find<UIActiveCache>();
    if (!cache) cache = &reg.ctx().emplace<UIActiveCache>();
    auto phase = reg.ctx().get<GameState>().phase;
    if (cache->valid && cache->phase == phase) return;

    auto view = reg.view<ActiveInPhase>();
    for (auto [e, aip] : view.each()) {
        const bool should_be_active = phase_active(aip, phase);
        const bool has_tag          = reg.all_of<ActiveTag>(e);
        if (should_be_active && !has_tag) {
            reg.emplace<ActiveTag>(e);
        } else if (!should_be_active && has_tag) {
            reg.remove<ActiveTag>(e);
        }
    }
    cache->phase = phase;
    cache->valid = true;
}
