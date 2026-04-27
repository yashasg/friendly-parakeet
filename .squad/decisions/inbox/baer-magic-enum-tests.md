# Decision: SFX Enum Test Contract After magic_enum Refactor

**Author:** Baer  
**Date:** 2026-04-26  
**Status:** RECOMMENDED — no approval needed (test-only change)

---

## Context

Keaton's magic_enum refactor removed the `SFX::COUNT` sentinel from the `SFX` enum.  
The `test_audio_system.cpp` file was previously excluded from the build via a CMakeLists regex.  
The queue-capacity test used `static_cast<SFX>(i % static_cast<int>(SFX::COUNT))` — now invalid.

---

## Decision

**`test_audio_system.cpp` is re-enabled in the build.** The `kAllSfx[]` explicit array pattern replaces the COUNT-based cycle.

### Pattern (in `test_audio_system.cpp`)

```cpp
constexpr SFX kAllSfx[] = {
    SFX::ShapeShift, SFX::BurnoutBank, SFX::Crash,    SFX::UITap,
    SFX::ChainBonus, SFX::ZoneRisky,   SFX::ZoneDanger, SFX::ZoneDead,
    SFX::ScorePopup, SFX::GameStart,
};
constexpr int kSfxCount = static_cast<int>(sizeof(kAllSfx) / sizeof(kAllSfx[0]));

static_assert(static_cast<int>(SFX::ShapeShift) == 0,
              "SFX must be zero-based (ShapeShift must equal 0)");
static_assert(static_cast<int>(magic_enum::enum_count<SFX>()) == kSfxCount,
              "SFX enum count mismatch — update kAllSfx when adding/removing SFX values");
```

### Rationale

| Old approach | New approach |
|---|---|
| `static_cast<SFX>(i % SFX::COUNT)` — COUNT is a sentinel, can produce COUNT as a value if loop overruns | `kAllSfx[i % kSfxCount]` — only real enum values used |
| No sync guard between test and enum | `static_assert(enum_count == kSfxCount)` — build fails if array goes stale |
| Relied on COUNT being last enumerator | Independent of sentinel existence |

---

## Scope of Change

- `CMakeLists.txt` — removed `test_audio_system` from EXCLUDE regex  
- `tests/test_audio_system.cpp` — contiguity guards + COUNT-free cycle  
- `tests/test_helpers_and_functions.cpp` — added `LanePushLeft`/`LanePushRight` to ObstacleKind ToString test

---

## Convention for Future SFX Additions

When a new `SFX` enumerator is added:

1. Add it to `kAllSfx[]` in `test_audio_system.cpp`
2. Add its spec to `SFX_SPECS` in `sfx_bank.cpp`
3. Update the `enum_count<SFX>() == N` guard in `audio.h`

The build will fail at static_assert if any of these are missed.
