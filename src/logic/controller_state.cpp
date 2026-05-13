#include "controller_state.hpp"

namespace HTLogic {

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
