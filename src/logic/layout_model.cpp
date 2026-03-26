#include "layout_model.hpp"

namespace HTLogic {

std::optional<GridPosition> moveInDirection(int x, int y, std::string_view direction) {
    if (direction == "up")
        return GridPosition {.x = x, .y = y - 1};
    if (direction == "down")
        return GridPosition {.x = x, .y = y + 1};
    if (direction == "right")
        return GridPosition {.x = x + 1, .y = y};
    if (direction == "left")
        return GridPosition {.x = x - 1, .y = y};

    return std::nullopt;
}

std::optional<GridPosition>
moveGridInDirection(int x, int y, std::string_view direction, int rows, int cols, bool loop) {
    if (rows <= 0 || cols <= 0)
        return std::nullopt;

    const auto moved = moveInDirection(x, y, direction);
    if (!moved.has_value())
        return std::nullopt;

    GridPosition result = *moved;
    if (loop) {
        result.x = (result.x + cols) % cols;
        result.y = (result.y + rows) % rows;
        return result;
    }

    if (result.x < 0 || result.x >= cols || result.y < 0 || result.y >= rows)
        return std::nullopt;

    return result;
}

WorkspaceID gridWorkspaceID(long view_id, int first_ws_offset, int rows, int cols, int x, int y) {
    const int ws_per_view = rows * cols;
    return view_id * ws_per_view + first_ws_offset + (view_id * rows + y) * cols + x + 1;
}

} // namespace HTLogic
