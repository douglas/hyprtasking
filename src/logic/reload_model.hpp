#pragma once

#include <vector>

namespace HTLogic {

using MonitorID = long;

struct ViewReloadDecision {
    bool hide_view = false;
    bool change_layout_now = false;
    bool change_layout_after_hide = false;
    bool reinitialize_position = false;
};

ViewReloadDecision decideViewReload(bool close_overview_on_reload, bool layout_changed, bool active);
std::vector<MonitorID>
missingMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids);
std::vector<MonitorID>
staleMonitorViewIDs(const std::vector<MonitorID>& view_ids, const std::vector<MonitorID>& monitor_ids);

} // namespace HTLogic
