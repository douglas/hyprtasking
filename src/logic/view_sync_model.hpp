#pragma once

#include <vector>

namespace HTLogic {

using MonitorID = long;

std::vector<MonitorID>
missingMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids);
std::vector<MonitorID>
staleMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids);

} // namespace HTLogic
