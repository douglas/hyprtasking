#pragma once

#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/macros.hpp>

namespace HTPlugin {

void initializeConfig();
bool registerCallbacks();
void unregisterCallbacks();
// removed_monitor_id is treated as gone even if Hyprland still lists it.
void registerMonitors(MONITORID removed_monitor_id = MONITOR_INVALID);
}
