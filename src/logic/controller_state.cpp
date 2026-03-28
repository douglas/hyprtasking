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

WorkspaceID resolveSelectionWorkspace(
    WorkspaceID selected_workspace_id,
    WorkspaceID hovered_workspace_id,
    WorkspaceID active_workspace_id
) {
    if (selected_workspace_id >= 0)
        return selected_workspace_id;
    if (hovered_workspace_id >= 0)
        return hovered_workspace_id;

    return active_workspace_id;
}

WorkspaceID
resolveNavigationSourceWorkspace(WorkspaceID selected_workspace_id, WorkspaceID active_workspace_id) {
    if (selected_workspace_id >= 0)
        return selected_workspace_id;

    return active_workspace_id;
}

WorkspaceID
resolveKeyboardSelectionWorkspace(WorkspaceID keyboard_workspace_id, WorkspaceID active_workspace_id) {
    if (keyboard_workspace_id >= 0)
        return keyboard_workspace_id;

    return active_workspace_id;
}

WorkspaceID resolveVisualWorkspaceID(
    bool hover_active,
    WorkspaceID hovered_workspace_id,
    WorkspaceID keyboard_workspace_id,
    WorkspaceID fallback_workspace_id
) {
    if (hover_active && hovered_workspace_id >= 0)
        return hovered_workspace_id;
    if (keyboard_workspace_id >= 0)
        return keyboard_workspace_id;

    return fallback_workspace_id;
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

MoveExecutionDecision resolveMoveExecution(bool move_window, bool has_hovered_window) {
    const bool move_hovered_window = shouldMoveHoveredWindow(move_window, has_hovered_window);
    const bool focus_moved_window = shouldFocusMovedWindow(move_window, has_hovered_window);

    return {
        .valid = !move_window || has_hovered_window,
        .move_hovered_window = move_hovered_window,
        .focus_moved_window = focus_moved_window,
        .use_move_window_warp = focus_moved_window,
    };
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
