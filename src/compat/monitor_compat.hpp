#pragma once

#if __has_include(<hyprland/src/output/Monitor.hpp>)
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
