#include "reload_model.hpp"

#include <algorithm>

namespace HTLogic {

ViewReloadDecision decideViewReload(
    bool close_overview_on_reload,
    bool layout_changed,
    bool active,
    bool closing,
    bool navigating
) {
    const bool runtime_active = active || closing || navigating;

    if (layout_changed) {
        if (runtime_active)
            return {.cancel_runtime_state = true, .change_layout_now = true};

        return {.change_layout_now = true};
    }

    if (close_overview_on_reload) {
        if (runtime_active)
            return {.cancel_runtime_state = true, .reinitialize_position = true};

        return {.reinitialize_position = true};
    }

    if (!runtime_active)
        return {.reinitialize_position = true};

    return {};
}

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
