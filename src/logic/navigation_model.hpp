#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace HTLogic {

using WorkspaceID = long;

enum class NavigateArgKind {
    Direction,
    Workspace,
    Invalid,
};

struct NavigateArg {
    NavigateArgKind kind = NavigateArgKind::Invalid;
    std::string direction;
    WorkspaceID workspace_id = -1;
};

NavigateArg parseNavigateArg(std::string_view arg);

struct GridPosition {
    int x = 0;
    int y = 0;
};

bool isGridDirection(std::string_view direction);

// Moves one cell up/down/left/right; wraps when loop is set, otherwise
// returns nullopt when leaving the grid or for an unknown direction.
std::optional<GridPosition>
moveGridInDirection(int x, int y, std::string_view direction, int rows, int cols, bool loop);

// Workspace id for cell (x, y) in a monitor's block of rows * cols workspaces.
WorkspaceID gridWorkspaceID(int block, int rows, int cols, int x, int y);

} // namespace HTLogic
