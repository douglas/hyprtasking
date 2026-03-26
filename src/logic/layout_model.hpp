#pragma once

#include <optional>
#include <string_view>

namespace HTLogic {

using WorkspaceID = long;

struct GridPosition {
    int x = 0;
    int y = 0;
};

std::optional<GridPosition> moveInDirection(int x, int y, std::string_view direction);
std::optional<GridPosition>
moveGridInDirection(int x, int y, std::string_view direction, int rows, int cols, bool loop);
WorkspaceID gridWorkspaceID(long view_id, int first_ws_offset, int rows, int cols, int x, int y);

} // namespace HTLogic
