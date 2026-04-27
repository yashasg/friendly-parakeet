# Decision: Safe Enum Macro — DECLARE_ENUM

**Author:** Keaton  
**Date:** 2026-05-17  
**Status:** PROPOSED — awaiting user approval before implementation  
**Blocks:** PR #43 must merge first; do not touch component headers until then

---

## Problem

- The existing X-macro pattern (`#define FOO_LIST(X) X(A) X(B) ...`) requires a separate named macro at the top of every header that needs it. Users must reference both the list macro and the expansion logic every time.
- The originally proposed 7-argument unscoped macro was rejected (Keyser + Kujan) for breaking `enum class`, underlying types, explicit values, fixed arity, and ODR.
- The user clarified: "the xmacro pattern is too cumbersome… I want a macro that takes the enum name as input."

---

## Hard Preprocessor Limit (be honest)

**The C++ preprocessor cannot derive enumerator names from an already-defined `enum class` by name alone.** There is no runtime reflection on enum members in C++20 without external tooling (libclang, magic_enum with compiler-specific builtins, etc.). The enumerator list MUST be provided at definition time — there is no way around this.

What we CAN do: accept the list inline in a single macro call that both defines the enum and generates its string table, eliminating the separate `FOO_LIST` macro entirely.

---

## Recommended API: `DECLARE_ENUM`

### Signature

```cpp
DECLARE_ENUM(EnumName, UnderlyingType, enumerator1, enumerator2, ...)
```

### Example Usage

```cpp
// Shape — replaces existing X-macro
DECLARE_ENUM(Shape, uint8_t, Circle, Square, Triangle, Hexagon)

// SFX — adds ToString(); COUNT sentinel included last
DECLARE_ENUM(SFX, uint8_t,
    ShapeShift, BurnoutBank, Crash, UITap, ChainBonus,
    ZoneRisky, ZoneDanger, ZoneDead, ScorePopup, GameStart, COUNT)

// HapticEvent — adds ToString() (13 enumerators, no gaps)
DECLARE_ENUM(HapticEvent, uint8_t,
    ShapeShift, LaneSwitch, JumpLand,
    Burnout1_5x, Burnout2_0x, Burnout3_0x, Burnout4_0x, Burnout5_0x,
    NearMiss, DeathCrash, NewHighScore, RetryTap, UIButtonTap)
```

`ToString()` output: `Shape::Circle → "Circle"`, `SFX::COUNT → "COUNT"`.

### Enums with explicit values — keep manual

```cpp
// GamePhase — explicit values document semantics; keep as-is
enum class GamePhase : uint8_t {
    Title        = 0,
    LevelSelect  = 1,
    Playing      = 2,
    Paused       = 3,
    GameOver     = 4,
    SongComplete = 5
};
// ToString added manually via switch (or the existing ui_source_resolver pattern)

// EndScreenChoice — same reasoning
enum class EndScreenChoice : uint8_t { None = 0, Restart = 1, LevelSelect = 2, MainMenu = 3 };
```

Explicit-value annotations serve as documentation of protocol/ABI stability. Removing them to use the macro is a net regression in clarity for these enums.

---

## Implementation

### New file: `app/util/enum_utils.h`

```cpp
#pragma once
// DECLARE_ENUM(Name, UnderlyingType, enumerators...)
//
// Generates:
//   enum class Name : UnderlyingType { enumerators... };
//   inline const char* ToString(Name) noexcept;
//
// Requirements:
//   - C++20 (__VA_OPT__ used for zero-overhead empty-arg safety)
//   - Enumerator values must be 0-based consecutive integers
//     (explicit = N assignments are not supported inline)
//   - For enums needing forward declarations, place the forward decl
//     manually: enum class Name : UnderlyingType;

// ── Internal FOR_EACH infrastructure ────────────────────────────────────────
// Recursive deferred expansion: handles up to 256 enumerators (4^4 expansions).
#define _SE_PARENS ()
#define _SE_EXPAND(...)  _SE_EX1(_SE_EX1(_SE_EX1(_SE_EX1(__VA_ARGS__))))
#define _SE_EX1(...)     _SE_EX2(_SE_EX2(_SE_EX2(_SE_EX2(__VA_ARGS__))))
#define _SE_EX2(...)     _SE_EX3(_SE_EX3(_SE_EX3(_SE_EX3(__VA_ARGS__))))
#define _SE_EX3(...)     __VA_ARGS__

#define _SE_MAP(f, ...)  __VA_OPT__(_SE_EXPAND(_SE_MAP_H(f, __VA_ARGS__)))
#define _SE_MAP_H(f, a, ...) f(a) __VA_OPT__(_SE_MAP_AGAIN _SE_PARENS (f, __VA_ARGS__))
#define _SE_MAP_AGAIN()  _SE_MAP_H

#define _SE_VAL(e)  e,
#define _SE_STR(e)  #e,

// ── Public macro ─────────────────────────────────────────────────────────────
#define DECLARE_ENUM(Name, Type, ...)                                       \
    enum class Name : Type {                                                \
        _SE_MAP(_SE_VAL, __VA_ARGS__)                                       \
    };                                                                      \
    inline const char* ToString(Name _e) noexcept {                        \
        static constexpr const char* const _s[] = {                        \
            _SE_MAP(_SE_STR, __VA_ARGS__)                                   \
        };                                                                  \
        auto _i = static_cast<std::size_t>(static_cast<Type>(_e));         \
        return _i < (sizeof(_s) / sizeof(_s[0])) ? _s[_i] : "???";        \
    }
```

### Why this is safe

| Concern | Status |
|---------|--------|
| `enum class` preserved | ✓ — generated verbatim |
| Underlying type (`:uint8_t`) preserved | ✓ — `Type` arg is injected |
| ODR / duplicate symbol | ✓ — `inline` function in header; one definition per TU guaranteed by C++17+ ODR for `inline` |
| `constexpr` string array | ✓ — zero heap allocation; resides in `.rodata` |
| Trailing comma in enum body | ✓ — valid C++11+ |
| Trailing comma in array initializer | ✓ — valid C++11+ |
| Mutable global (the original macro's `const char*[]`) | ✗ eliminated — `constexpr const char* const` is fully immutable |
| Variable arity | ✓ — `__VA_OPT__` + recursive expansion |
| Empty enum guard | ✓ — `__VA_OPT__` suppresses expansion on zero args |
| Zero warnings | ✓ — no implicit conversions; `static_cast` is explicit |
| Forward declarations | ⚠ — `DECLARE_ENUM` always produces a full definition. Write `enum class WindowPhase : uint8_t;` manually in `player.h`; `rhythm.h` uses `DECLARE_ENUM`. One-line cost, not a regression. |

### Why explicit values are excluded

Supporting `(Name, Value)` tuple unpacking in variadic macros requires either:
- `BOOST_PP_SEQ_FOR_EACH` (not in this project)
- An additional layer of parenthesized-pair parsing macros (3–4 extra helpers per tuple element type)

For only 5 enums in the codebase that have meaningful explicit values, that complexity is not justified. Those enums (GamePhase, EndScreenChoice, BurnoutZone, WindowPhase, Layer, VMode, FontSize) keep manual definitions. The `= N` annotations carry semantic weight; preserving them is a feature, not a bug.

---

## Which Enums Convert, Which Stay Manual

### Convert to `DECLARE_ENUM` (replace X-macro or add ToString)

| Enum | File | Values | Action |
|------|------|--------|--------|
| `Shape` | `player.h` | 4 | Replace `SHAPE_LIST` X-macro |
| `ObstacleKind` | `obstacle.h` | 8 | Replace `OBSTACLE_KIND_LIST` X-macro |
| `TimingTier` | `rhythm.h` | 4 | Replace `TIMING_TIER_LIST` X-macro |
| `SFX` | `audio.h` | 10 + COUNT | Add `ToString()` |
| `HapticEvent` | `haptics.h` | 13 | Add `ToString()` |
| `ActiveScreen` | `ui_state.h` | 6 | Add `ToString()` (debug/logging) |
| `DeathCause` | `song_state.h` | 4 | Add `ToString()` (replace `death_cause_to_string`) |

### Stay manual (explicit values, forward-decl, or no ToString needed)

| Enum | Reason |
|------|--------|
| `GamePhase` | Explicit values document ABI/protocol; manual `ToString()` switch in `ui_source_resolver.cpp` is fine |
| `EndScreenChoice` | Explicit values, no logging need |
| `WindowPhase` | Forward-declared in `player.h`; definition stays in `rhythm.h` |
| `BurnoutZone` | Explicit values (semantically important: 0=None…4=Dead); add manual `ToString()` if needed |
| `Layer` | Explicit values |
| `VMode` | Explicit values |
| `FontSize` | `int` underlying type (not `uint8_t`) |
| `InputSource`, `Direction`, `TextAlign` | Pure data, no ToString needed |
| `InputType`, `MeshType`, `MenuActionKind` | Pure data, no ToString needed |
| `ProceduralWave` | In `.cpp` (sfx_bank.cpp), not a header enum; leave in place |

---

## Does This Replace All Enums?

**No.** `DECLARE_ENUM` targets only enums that need `ToString()` and have 0-based consecutive values. Enums that are pure data or have meaningful explicit value assignments stay as plain `enum class`. Approximately 7 of 23 enums convert.

---

## Scope Fit With PR #43

PR #43 (ecs_refactor) touches `player.h`, `rendering.h`, `rhythm.h`, and several component headers — exactly the files this refactor would touch. **Do not start implementation until PR #43 merges.** All 7 target headers are in the conflict zone.

---

## Implementation Plan (post-PR #43 merge)

| Step | Owner | Action |
|------|-------|--------|
| 1 | Keaton | Create `app/util/enum_utils.h` with macro + internal helpers |
| 2 | Keaton | Convert `Shape`, `ObstacleKind`, `TimingTier` (X-macro replacements; safest first) |
| 3 | Keaton | Convert `SFX`, `HapticEvent` (add ToString) |
| 4 | Keaton | Convert `ActiveScreen`, `DeathCause` (replace `death_cause_to_string` in `ui_source_resolver.cpp`) |
| 5 | Baer | Zero-warnings build on macOS (arm64) + WASM; run full test suite |
| 6 | Kujan | Review pass |

**Files touched:** `app/util/enum_utils.h` (new), `app/components/player.h`, `app/components/obstacle.h`, `app/components/rhythm.h`, `app/components/audio.h`, `app/components/haptics.h`, `app/components/ui_state.h`, `app/components/song_state.h`, `app/systems/ui_source_resolver.cpp`.  
No system `.cpp` files change behavior; `death_cause_to_string` in `ui_source_resolver.cpp` is the only logic migration.

---

## Acceptance Tests / Build Plan

```bash
# After implementation:
cmake -B build -S . -Wno-dev
cmake --build build --parallel   # must produce zero warnings

./build/shapeshifter_tests        # all existing tests must pass
# Spot-check ToString outputs:
# ToString(Shape::Circle)      == "Circle"
# ToString(SFX::COUNT)         == "COUNT"
# ToString(HapticEvent::DeathCrash) == "DeathCrash"
# ToString(ObstacleKind::LanePushRight) == "LanePushRight"
```

Additional manual check: confirm `SFXBank::SFX_COUNT` still equals `static_cast<int>(SFX::COUNT)` after conversion (it does — implicit value assignment is preserved).

---

## Comparison With Prior Recommendations

| Approach | By | Verdict |
|----------|----|---------|
| Fixed-7-arg unscoped macro | User (initial) | Rejected — breaks enum class, types, arity, ODR |
| X-macro `FOO_LIST(X)` (existing pattern) | Keyser | Works but user finds too cumbersome |
| `DECLARE_ENUM` variadic C++20 | Keaton | **Recommended** — single definition site, enum class preserved, zero ODR issues, variable arity |
