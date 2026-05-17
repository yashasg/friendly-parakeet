// raygui single-implementation TU.
//
// raygui is header-only and must have `RAYGUI_IMPLEMENTATION` defined in
// exactly one translation unit. This file is that TU. Build-system glue
// in `CMakeLists.txt` defines the macro for this source and excludes it
// from unity builds so the single-definition rule holds.
//
// Lives in `app/util/` per issue #1317 sub-PR 1 (closes `app/ui/` per
// .squad/decisions.md § 7 folder allowlist): this file is build-glue
// util, not a UI screen.

#include <raygui.h>
