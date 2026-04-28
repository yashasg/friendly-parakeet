---
name: "entt-enum-bitmask"
description: "How to use entt::enum_as_bitmask for typed bitfield enums in this codebase"
domain: "ecs, c++, entt"
confidence: "high"
source: "earned"
---

## Context

When an `enum class` represents a set of flags (not a single state), EnTT provides compile-time-activated typed `|`, `&`, `^`, `~`, `!`, and compound-assignment operators via `entt/core/enum.hpp`. This replaces hand-rolled `uint8_t` shift helpers with type-safe, IDE-navigable operators.

**When to use:** Any component field that stores a bitmask of enum values rather than a single discriminant.

**When NOT to use:** State-machine enums that hold a single value (e.g. `GamePhase`). Keep those sequential; introduce a parallel power-of-two enum for the mask.

## Patterns

### 1. Define the enum with power-of-two values + sentinel

```cpp
// In game_state.h (or the relevant component header)
enum class GamePhaseBit : uint8_t {
    Title        = 1 << 0,
    LevelSelect  = 1 << 1,
    Playing      = 1 << 2,
    Paused       = 1 << 3,
    GameOver     = 1 << 4,
    SongComplete = 1 << 5,
    _entt_enum_as_bitmask   // sentinel — no value needed, just presence
};
```

`_entt_enum_as_bitmask` is detected by `entt::enum_as_bitmask<T>` via `std::void_t`. No `#include` beyond `<entt/entt.hpp>` is required.

### 2. Use typed field in component

```cpp
struct ActiveInPhase {
    GamePhaseBit phase_mask = GamePhaseBit{};  // default: no bits set
};
```

Default-construct `GamePhaseBit{}` for an empty mask (zero underlying value).

### 3. Bridge helper for state→bit conversion

```cpp
[[nodiscard]] constexpr GamePhaseBit to_phase_bit(GamePhase p) noexcept {
    return static_cast<GamePhaseBit>(uint8_t{1} << static_cast<uint8_t>(p));
}
```

### 4. Spawn sites use typed literals and |

```cpp
// Single phase
reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Playing);

// Multiple phases
const GamePhaseBit mask = GamePhaseBit::GameOver | GamePhaseBit::SongComplete;
reg.emplace<ActiveInPhase>(card, mask);
```

### 5. Phase check uses !! double-not idiom

```cpp
inline bool phase_active(const ActiveInPhase& aip, GamePhase phase) {
    return !!(aip.phase_mask & to_phase_bit(phase));
}
```

`entt::operator!(GamePhaseBit)` returns `bool` (true if zero). `!!` inverts twice: "non-zero → true".

## Examples

- `app/components/game_state.h` — `GamePhaseBit` definition + `to_phase_bit`
- `app/components/input_events.h` — `ActiveInPhase`, `phase_active()`
- `app/systems/ui_button_spawner.h` — spawn sites using typed literals
- `tests/test_components.cpp` — `[phase_mask]` test cases

## Anti-Patterns

- **Don't mix `uint8_t` and `GamePhaseBit`**: once the field is typed, all sites must use the typed enum. Implicit constructors are not provided.
- **Don't add `_entt_enum_as_bitmask` to state-machine enums**: `GamePhase::Title | GamePhase::Playing` is meaningless and would compile, producing confusing bugs.
- **Don't use `static_cast<uint8_t>(mask) != 0`** when `!!(mask & bit)` is available — prefer the typed operators.
