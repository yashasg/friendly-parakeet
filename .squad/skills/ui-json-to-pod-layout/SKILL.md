# SKILL: Hot-Path JSON-to-POD Layout Cache

## Problem

UI render systems that parse JSON field values each frame (via `json["key"].get<float>()`)
waste CPU on repeated dictionary lookups for stable layout constants that never change
during a screen session.

## Solution Pattern

Extract stable layout fields from JSON at screen-load time into plain POD structs stored in
`reg.ctx()`. The render path reads only from the POD struct — zero JSON access per frame.

## Structure

### 1. POD Struct (component header)

```cpp
// app/components/ui_layout_cache.h
struct HudLayout {
    float btn_w{};          // pre-multiplied: x_n * SCREEN_W_F
    float btn_y{};          // pre-multiplied: y_n * SCREEN_H_F
    Color active_bg{};      // resolved once from JSON array
    bool  valid{false};     // false = required elements absent; render returns early
};
```

All coordinate fields MUST be pre-multiplied by `SCREEN_W_F` / `SCREEN_H_F` at build time.
This eliminates both the JSON access AND the per-frame multiplication.

### 2. Builder Function (ui_loader.cpp)

```cpp
HudLayout build_hud_layout(const UIState& ui) {
    using namespace entt::literals;
    HudLayout layout{};

    // O(1) lookup via #312 element map
    const auto* el = find_layout_el(ui, "shape_buttons"_hs);
    if (!el) return layout;  // valid stays false

    try {
        // Use .at() NOT operator[] — operator[] on const json uses assert(), not exceptions
        layout.btn_w = el->at("button_w_n").get<float>() * constants::SCREEN_W_F;
        layout.btn_y = el->at("y_n").get<float>()        * constants::SCREEN_H_F;
        layout.active_bg = json_color_rl(el->at("active_bg"));
    } catch (const nlohmann::json::exception& e) {
        std::fprintf(stderr, "[WARN] build_hud_layout: %s\n", e.what());
        return layout;  // valid stays false
    }

    layout.valid = true;
    return layout;
}
```

**Critical:** Always use `.at("key")` for required fields inside try-catch blocks.
`operator["key"]` on a `const nlohmann::json&` triggers `assert()` (SIGABRT), not an exception.

### 3. Store at Screen Load (ui_navigation_system.cpp)

```cpp
if (screen_changed) {
    build_ui_element_map(ui);
    // Use insert_or_assign on screen-change boundaries so the cache is
    // always replaced (not silently skipped if a previous value exists).
    if (screen_name == "gameplay") {
        reg.ctx().insert_or_assign(build_hud_layout(ui));
    }
}
```

### 4. Consume in Render (ui_render_system.cpp)

```cpp
static void draw_hud(const entt::registry& reg, const HudLayout& layout) {
    if (!layout.valid) return;
    float btn_w = layout.btn_w;   // no JSON, no multiply
    // ...
}

// In ui_render_system():
const auto* hud = reg.ctx().find<HudLayout>();
if (hud) draw_hud(reg, *hud);
```

## Key Rules

| Rule | Detail |
|------|--------|
| Use `.at()` not `[]` | `[]` on const json asserts; `.at()` throws catchable exceptions |
| Pre-multiply coordinates | Store `x_n * SCREEN_W_F`, not raw `x_n` |
| `valid` guard | Render function returns early if `!layout.valid`; no silent garbage |
| EnTT ctx — startup insert | `ctx().emplace<T>(...)` — valid for one-time initialization where the type is absent; errors if already present |
| EnTT ctx — upsert/reset | `ctx().insert_or_assign(value)` — use at session-restart or screen-change boundaries where the type may already exist and must be replaced |
| Build boundary | Call after `build_ui_element_map()` on screen change |

## Example: Color extraction helper

```cpp
static Color json_color_rl(const nlohmann::json& arr) {
    uint8_t a = arr.size() > 3 ? arr[3].get<uint8_t>() : 255;
    return {arr[0].get<uint8_t>(), arr[1].get<uint8_t>(), arr[2].get<uint8_t>(), a};
}
```

Works for both `[r,g,b]` and `[r,g,b,a]` arrays.

## Applied In

- `#322`: `HudLayout` (gameplay screen) and `LevelSelectLayout` (level_select screen)
- Files: `app/components/ui_layout_cache.h`, `app/systems/ui_loader.{h,cpp}`,
  `app/systems/ui_navigation_system.cpp`, `app/systems/ui_render_system.cpp`
- Tests: `tests/test_ui_layout_cache.cpp`
