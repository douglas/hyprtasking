#include "navigation_model.hpp"

#include <cctype>
#include <charconv>

namespace {

std::string_view trim_view(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return value;
}

} // namespace

namespace HTLogic {

NavigateArg parseNavigateArg(std::string_view arg) {
    const std::string_view trimmed = trim_view(arg);
    if (trimmed == "up" || trimmed == "down" || trimmed == "left" || trimmed == "right") {
        return {
            .kind = NavigateArgKind::Direction,
            .direction = std::string(trimmed),
        };
    }

    WorkspaceID workspace_id = -1;
    const auto* begin = trimmed.data();
    const auto* end = trimmed.data() + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, workspace_id);
    if (ec == std::errc {} && ptr == end && workspace_id > 0) {
        return {
            .kind = NavigateArgKind::Workspace,
            .workspace_id = workspace_id,
        };
    }

    return {.kind = NavigateArgKind::Invalid};
}

bool isGridDirection(std::string_view direction) {
    return direction == "up" || direction == "down" || direction == "left" || direction == "right";
}

std::optional<GridPosition>
moveGridInDirection(int x, int y, std::string_view direction, int rows, int cols, bool loop) {
    if (rows <= 0 || cols <= 0)
        return std::nullopt;

    if (direction == "up")
        y--;
    else if (direction == "down")
        y++;
    else if (direction == "right")
        x++;
    else if (direction == "left")
        x--;
    else
        return std::nullopt;

    if (loop)
        return GridPosition {.x = (x + cols) % cols, .y = (y + rows) % rows};

    if (x < 0 || x >= cols || y < 0 || y >= rows)
        return std::nullopt;

    return GridPosition {.x = x, .y = y};
}

WorkspaceID gridWorkspaceID(int block, int rows, int cols, int x, int y) {
    return static_cast<WorkspaceID>(block * rows + y) * cols + x + 1;
}

} // namespace HTLogic
