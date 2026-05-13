#include "view_sync_model.hpp"

#include <algorithm>

namespace HTLogic {

std::vector<MonitorID>
missingMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids) {
    std::vector<MonitorID> missing_ids;
    for (const auto monitor_id : monitor_ids) {
        if (std::ranges::find(view_ids, monitor_id) == view_ids.end())
            missing_ids.push_back(monitor_id);
    }

    return missing_ids;
}

std::vector<MonitorID>
staleMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids) {
    std::vector<MonitorID> stale_ids;
    for (const auto view_id : view_ids) {
        if (std::ranges::find(monitor_ids, view_id) == monitor_ids.end())
            stale_ids.push_back(view_id);
    }

    return stale_ids;
}

} // namespace HTLogic
