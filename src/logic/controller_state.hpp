#pragma once

#include <optional>

namespace HTLogic {

using WorkspaceID = long;

struct DropWorkspaceDecision {
    bool        valid = false;
    WorkspaceID workspace_id = -1;
    bool        create_if_missing = false;
    bool        snap_to_workspace = false;
};

WorkspaceID resolveSelectionWorkspace(
    WorkspaceID selected_workspace_id,
    WorkspaceID hovered_workspace_id,
    WorkspaceID active_workspace_id
);
WorkspaceID
resolveKeyboardSelectionWorkspace(WorkspaceID keyboard_workspace_id, WorkspaceID active_workspace_id);
WorkspaceID resolveVisualWorkspaceID(
    bool hover_active,
    WorkspaceID hovered_workspace_id,
    WorkspaceID keyboard_workspace_id,
    WorkspaceID fallback_workspace_id
);
DropWorkspaceDecision
resolveDropWorkspace(WorkspaceID hovered_workspace_id, std::optional<WorkspaceID> dragged_workspace_id);

} // namespace HTLogic
