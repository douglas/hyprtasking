#pragma once

#include <optional>

#include "layout_model.hpp"

namespace HTLogic {

struct DropWorkspaceDecision {
    bool        valid = false;
    WorkspaceID workspace_id = -1;
    bool        create_if_missing = false;
    bool        snap_to_workspace = false;
};

WorkspaceID resolveExitWorkspaceID(bool use_hovered_workspace, WorkspaceID hovered_workspace_id, WorkspaceID active_workspace_id);
WorkspaceID
resolveMoveSourceWorkspace(bool move_window, WorkspaceID active_workspace_id, std::optional<WorkspaceID> hovered_workspace_id);
bool shouldMoveHoveredWindow(bool move_window, bool has_hovered_window);
bool shouldFocusMovedWindow(bool move_window, bool has_hovered_window);
DropWorkspaceDecision
resolveDropWorkspace(WorkspaceID hovered_workspace_id, std::optional<WorkspaceID> dragged_workspace_id);

} // namespace HTLogic
