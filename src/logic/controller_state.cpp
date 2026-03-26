#include "controller_state.hpp"

namespace HTLogic {

WorkspaceID resolveExitWorkspaceID(
    bool use_hovered_workspace,
    WorkspaceID hovered_workspace_id,
    WorkspaceID active_workspace_id
) {
    if (use_hovered_workspace && hovered_workspace_id >= 0)
        return hovered_workspace_id;

    return active_workspace_id;
}

WorkspaceID resolveMoveSourceWorkspace(
    bool move_window,
    WorkspaceID active_workspace_id,
    std::optional<WorkspaceID> hovered_workspace_id
) {
    if (!move_window)
        return active_workspace_id;

    if (!hovered_workspace_id.has_value())
        return -1;

    return *hovered_workspace_id;
}

bool shouldMoveHoveredWindow(bool move_window, bool has_hovered_window) {
    return move_window && has_hovered_window;
}

bool shouldFocusMovedWindow(bool move_window, bool has_hovered_window) {
    return move_window && has_hovered_window;
}

DropWorkspaceDecision
resolveDropWorkspace(WorkspaceID hovered_workspace_id, std::optional<WorkspaceID> dragged_workspace_id) {
    if (hovered_workspace_id >= 0) {
        return {
            .valid = true,
            .workspace_id = hovered_workspace_id,
            .create_if_missing = true,
            .snap_to_workspace = false,
        };
    }

    if (dragged_workspace_id.has_value()) {
        return {
            .valid = true,
            .workspace_id = *dragged_workspace_id,
            .create_if_missing = false,
            .snap_to_workspace = true,
        };
    }

    return {};
}

} // namespace HTLogic
