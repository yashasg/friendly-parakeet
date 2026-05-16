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
// In test_player.h (or the relevant component/system header)
enum class ActionDoneBit : uint8_t {
    Shape    = 1 << 0,
    Lane     = 1 << 1,
    Vertical = 1 << 2,
    _entt_enum_as_bitmask   // sentinel — no value needed, just presence
};
```

`_entt_enum_as_bitmask` is detected by `entt::enum_as_bitmask<T>` via `std::void_t`. No `#include` beyond `<entt/entt.hpp>` is required.

### 2. Use typed field in a value type or component

```cpp
struct TestPlayerAction {
    // …other fields…
    ActionDoneBit done_flags = ActionDoneBit{};  // default: no bits set
};
```

Default-construct `ActionDoneBit{}` for an empty mask (zero underlying value).

### 3. Check a bit using the !! double-not idiom

```cpp
static bool test_player_shape_done(const TestPlayerAction& action) {
    return !!(action.done_flags & ActionDoneBit::Shape);
}
```

`entt::operator!(ActionDoneBit)` returns `bool` (true if zero). `!!` inverts twice: "non-zero → true".

### 4. Set a bit using compound-assign |=

```cpp
static void test_player_mark_shape_done(TestPlayerAction& action) {
    action.done_flags |= ActionDoneBit::Shape;
}
```

### 5. Combine bits with typed |

```cpp
const ActionDoneBit all_handled =
    ActionDoneBit::Shape | ActionDoneBit::Lane | ActionDoneBit::Vertical;
```

## Examples

- `app/systems/test_player.h` — `ActionDoneBit` definition + `TestPlayerAction::done_flags`
- `app/systems/test_player_system.cpp` — `&` read sites and `|=` write sites

## Anti-Patterns

- **Don't mix `uint8_t` and the typed enum**: once the field is typed, all sites must use the typed enum. Implicit constructors are not provided.
- **Don't add `_entt_enum_as_bitmask` to state-machine enums** (single-value discriminants such as `GamePhase`): `Phase::A | Phase::B` is meaningless and would compile, producing confusing bugs. Per `#1202`/`#1204`, prefer per-value tags over discriminator enums entirely; only use this skill when a *true* bitmask is the right model.
- **Don't use `static_cast<uint8_t>(mask) != 0`** when `!!(mask & bit)` is available — prefer the typed operators.
