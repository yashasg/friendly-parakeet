### 2026-04-27 — Verbal: test cleanup for burnout removal (#239)

**Decision:** Delete `test_burnout_system.cpp` and `test_burnout_bank_on_action.cpp` entirely (not stub them) since CMakeLists uses `file(GLOB TEST_SOURCES tests/*.cpp)` — deleted files are automatically excluded.

**Decision:** `test_helpers.h :: make_registry()` still emplace `BurnoutState` because `test_haptic_system.cpp`, `test_death_model_unified.cpp`, and `test_components.cpp` still reference it. Removing it now would break those tests before McManus removes the component header. McManus/Hockney own that cleanup.

**Decision:** Added `[no_burnout]` tag to the new no-penalty test so future runs can target it in isolation.

**Out-of-scope flag:** `test_haptic_system.cpp` has 5 `burnout_system()` call sites (lines 75, 87, 99, 112, 125) that will break when McManus removes `burnout_system`. These need a follow-up task for Hockney or McManus.
