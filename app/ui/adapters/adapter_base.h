// RGuiLayout adapter template - eliminates init/render boilerplate across adapters.
// Uses C++17 auto template parameters for compile-time dispatch to generated layout functions.

#ifndef ADAPTER_BASE_H
#define ADAPTER_BASE_H

#include <entt/entt.hpp>
#include <raylib.h>

// Template adapter: manages layout state lifecycle and provides uniform render interface.
// LayoutState: generated state struct (e.g., GameOverLayoutState)
// InitFunc: pointer to generated Init function
// RenderFunc: pointer to generated Render function
template<typename LayoutState, auto InitFunc, auto RenderFunc>
class RGuiAdapter {
    LayoutState state_;
    bool initialized_ = false;

public:
    void init() {
        if (!initialized_) {
            state_ = InitFunc();
            initialized_ = true;
        }
    }

    void render() {
        if (!initialized_) {
            init();
        }
        RenderFunc(&state_);
    }

    LayoutState& state() { return state_; }
    const LayoutState& state() const { return state_; }
};

// Helper: construct Rectangle with anchor-relative offsets.
// Reduces manual arithmetic in settings adapter and similar dynamic-label cases.
inline Rectangle offset_rect(Vector2 anchor, float x, float y, float w, float h) {
    return (Rectangle){anchor.x + x, anchor.y + y, w, h};
}

#endif // ADAPTER_BASE_H
