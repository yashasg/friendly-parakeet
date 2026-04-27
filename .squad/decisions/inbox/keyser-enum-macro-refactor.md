# Decision Recommendation: Enum Macro Refactor

**Author:** Keyser  
**Date:** 2026-05-17  
**Status:** NO-GO on exact macro — propose safer alternative  
**Blocks:** PR #43 must merge first

---

## Request

Replace all codebase enums with a fixed 7-argument unscoped macro:

```c
#define ENUM_MACRO(name, v1, v2, v3, v4, v5, v6, v7) \
    enum name { v1, v2, v3, v4, v5, v6, v7 };         \
    const char *name##Strings[] = { #v1, #v2, #v3, #v4, #v5, #v6, #v7 };
```

---

## Finding: Zero enums can be converted directly

Every enum in this codebase fails at least one compatibility test with the proposed macro.

### Incompatibility Matrix

| Enum | File | Count | Explicit values | enum class | uint8_t | Forward decl | X-macro already | Verdict |
|------|------|-------|----------------|-----------|---------|--------------|----------------|---------|
| TestPlayerSkill | test_player.h | 3 | ✓ (0,1,2) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| UIElementType | ui_element.h | 4 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| ProceduralWave | sfx_bank.cpp | 4 | — | ✓ | ✓ | — | — | ❌ 2; in .cpp not header |
| SFX | audio.h | **11** | ✓ (ShapeShift=0) | ✓ | ✓ | — | — | ❌ 3; COUNT sentinel |
| ObstacleKind | obstacle.h | **8** | — | ✓ | ✓ | — | **✓** | ❌ 3; already X-macro |
| InputType | input_events.h | 2 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| MenuActionKind | input_events.h | 7 | ✓ (0–6) | ✓ | ✓ | — | — | ❌ 2 (class + explicit values) |
| DeathCause | song_state.h | 4 | ✓ (0–3) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| ActiveScreen | ui_state.h | 6 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| TextAlign | text.h | 3 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| FontSize | text.h | 3 | ✓ (0,1,2) | ✓ | **int** | — | — | ❌ 3; int not uint8_t |
| InputSource | input.h | 3 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| Direction | input.h | 4 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| Shape | player.h | 4 | — | ✓ | ✓ | — | **✓** | ❌ 3; already X-macro |
| VMode | player.h | 3 | ✓ (0,1,2) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| Layer | rendering.h | 4 | ✓ (0–3) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| MeshType | rendering.h | 3 | — | ✓ | ✓ | — | — | ❌ 2 incompatibilities |
| HapticEvent | haptics.h | **13** | ✓ (first=0) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| GamePhase | game_state.h | 6 | ✓ (all) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| EndScreenChoice | game_state.h | 4 | ✓ (all) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| BurnoutZone | burnout.h | 5 | ✓ (all) | ✓ | ✓ | — | — | ❌ 3 incompatibilities |
| WindowPhase | rhythm.h | 4 | ✓ (all) | ✓ | ✓ | **✓ player.h** | — | ❌ 4 incompatibilities |
| TimingTier | rhythm.h | 4 | — | ✓ | ✓ | — | **✓** | ❌ 3; already X-macro |

### Root Incompatibilities (structural — affect ALL enums)

1. **`enum class` → plain `enum`** — Every usage site uses scoped names (`Shape::Circle`, `GamePhase::Playing`, etc.). Plain `enum` removes the scope qualifier, requiring a global rename of every call site or introducing silent name collisions. With `-Wall -Wextra -Werror`, shadowing warnings would become build failures.

2. **No underlying type** — All enums specify `: uint8_t` (one uses `: int`). The macro produces a compiler-determined underlying type. This breaks ABI stability, storage-size assumptions in structs, and will trigger `-Wconversion` warnings = build failure on our zero-warnings policy.

3. **Fixed 7 arguments** — Only `MenuActionKind` has exactly 7 enumerators, and even it has explicit assignments (`= 0` through `= 6`) the macro doesn't support. All other enums have 2–13 members.

4. **`const char *name##Strings[]` is a mutable non-const global array** — Three enums (`ObstacleKind`, `Shape`, `TimingTier`) already have `ToString()` functions via X-macros. The proposed string array would be a regression: mutable global state, no compile-time lookup, duplicate symbol risk across TUs.

5. **Forward declaration** — `WindowPhase` is forward-declared in `player.h`. A macro cannot produce a forward declaration; any TU that includes `player.h` before `rhythm.h` currently compiles. The macro would break this.

---

## Recommendation: NO-GO on exact macro

**Verdict: The proposed macro cannot be applied to any enum in this codebase without breaking compilation, ABI, or the zero-warnings policy.**

---

## Minimal Safe Alternative

The spirit of the request — enums with built-in string tables — is already addressed for three enums via the project's existing X-macro pattern. Extend that pattern to remaining enums that actually need string serialization.

```cpp
// Pattern already in use (obstacle.h, player.h, rhythm.h):
#define THING_LIST(X) X(A) X(B) X(C)
enum class Thing : uint8_t {
    #define THING_ENUM(name) name,
    THING_LIST(THING_ENUM)
    #undef THING_ENUM
};
inline const char* ToString(Thing t) {
    switch (t) {
        #define THING_STR(name) case Thing::name: return #name;
        THING_LIST(THING_STR)
        #undef THING_STR
    }
    return "???";
}
```

This pattern:
- Preserves `enum class` and underlying types ✓
- Preserves explicit values where needed (add after the `name` in the X) ✓
- Compiles with zero warnings ✓
- Handles any enumerator count ✓
- Supports forward declarations ✓
- String table is static const (no mutable global) ✓

If a broader utility header is desired, a variadic-safe project macro wrapping this pattern is appropriate, but **not** the 7-argument unscoped form requested.

---

## PR #43 Constraint

**Wait for PR #43 to merge first.** Active review-fix agents are still running on that PR. The files most likely in flight (`player.h`, `rhythm.h`, `rendering.h`, component headers) are the same headers this refactor touches. Starting now risks merge conflicts on every modified file.

---

## Implementation Sequence (if approved, after PR #43 merges)

1. **Keyser** — Finalize X-macro helper convention (add to `CONTRIBUTING.md` or `design-docs/architecture.md`). Identify which enums actually need string tables (not all do).
2. **McManus** — Apply X-macro pattern to enums that lack `ToString()` and are used in debug/logging paths: `GamePhase`, `ActiveScreen`, `BurnoutZone`, `DeathCause`, `HapticEvent`, `VMode`, `Layer`, `WindowPhase`, `TimingTier` (already done), `Shape` (already done).
3. **Baer** — Verify zero-warnings clean build on all 4 platforms after each file is touched. Run full test suite (386 tests).
4. **Kujan** — Review pass before merge.

**Enums that do NOT need string tables** (pure data, never logged/displayed): `InputType`, `InputSource`, `Direction`, `MeshType`, `EndScreenChoice`, `MenuActionKind`. Leave as-is.

---

## Files That Would Be Touched

~14 headers + 1 .cpp file (sfx_bank.cpp for ProceduralWave, if included). All in `app/components/` plus `app/systems/sfx_bank.cpp`.

No system `.cpp` files need changes — all enum definitions are in headers.
