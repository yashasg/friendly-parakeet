#pragma once

// Back-compat shim. The original `entities/settings.h` god-file split into
// component data, an entity factory, and a system per #1194 / #1195.
// New code should include the canonical headers directly:
//   - components/settings.h        — SettingsState, SettingsPersistence
//   - entities/settings_entity.h   — create/destroy_settings_entity()
//   - systems/settings_system.h    — accessors, namespace settings { … }
#include "../components/settings.h"
#include "settings_entity.h"
#include "../systems/settings_system.h"
