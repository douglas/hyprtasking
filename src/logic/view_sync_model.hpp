#pragma once

#include <optional>
#include <vector>

namespace HTLogic {

using MonitorID = long;

std::optional<int>
monitorGridIndex(MonitorID monitor_id, const std::vector<MonitorID>& monitor_ids);

std::vector<MonitorID>
missingMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids);
std::vector<MonitorID>
staleMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids);

} // namespace HTLogic
