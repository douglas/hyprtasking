#pragma once

#include "../build_contract.hpp"

// Gate on the Hyprland version being built against, not on which headers
// happen to be on the include path.
#if HT_HYPRLAND_GE_0_56
    #include <hyprland/src/output/Monitor.hpp>
namespace HTCompat {
using MonitorClass = Monitor::CMonitor;
}
#else
#include <hyprland/src/helpers/Monitor.hpp>
namespace HTCompat {
using MonitorClass = CMonitor;
}
#endif
