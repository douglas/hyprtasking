#pragma once

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

} // namespace HTLogic
