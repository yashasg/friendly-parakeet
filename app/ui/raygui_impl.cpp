// raygui single-implementation TU.
//
// raygui is header-only and must have `RAYGUI_IMPLEMENTATION` defined in
// exactly one translation unit. This file is that TU. Build-system glue
// in `CMakeLists.txt` defines the macro for this source and excludes it
// from unity builds so the single-definition rule holds.
//
// Replaces the same role previously held by
// `app/ui/screen_controllers/title_screen_controller.cpp` before that
// folder was deleted (issue #1308 / refs #1287 OoS-B / #1193).

#include <raygui.h>
