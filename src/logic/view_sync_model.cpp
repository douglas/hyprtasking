#include "view_sync_model.hpp"

#include <algorithm>
#include <iterator>

namespace HTLogic {

std::vector<std::string> assignMonitorBlocks(
    std::vector<std::string> blocks,
    const std::vector<std::string>& connected_names
) {
    const auto is_connected = [&](const std::string& name) {
        return std::ranges::find(connected_names, name) != connected_names.end();
    };

    for (const auto& name : connected_names) {
        if (std::ranges::find(blocks, name) != blocks.end())
            continue;

        const auto free_slot = std::ranges::find_if(blocks, [&](const std::string& owner) {
            return !is_connected(owner);
        });
        if (free_slot != blocks.end())
            *free_slot = name;
        else
            blocks.push_back(name);
    }

    return blocks;
}

std::optional<int>
monitorBlockIndex(const std::vector<std::string>& blocks, const std::string& monitor_name) {
    const auto it = std::ranges::find(blocks, monitor_name);
    if (it == blocks.end())
        return std::nullopt;

    return static_cast<int>(std::distance(blocks.begin(), it));
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
