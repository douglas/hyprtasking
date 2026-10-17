#pragma once

#include <optional>
#include <string>
#include <vector>

namespace HTLogic {

using MonitorID = long;

// Each slot is one workspace block, owned by a monitor name. Connected monitors
// keep their slot; new ones take the lowest slot not held by a connected monitor.
std::vector<std::string> assignMonitorBlocks(
    std::vector<std::string> blocks,
    const std::vector<std::string>& connected_names
);
std::optional<int>
monitorBlockIndex(const std::vector<std::string>& blocks, const std::string& monitor_name);

std::vector<MonitorID>
missingMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids);
std::vector<MonitorID>
staleMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids);

} // namespace HTLogic
