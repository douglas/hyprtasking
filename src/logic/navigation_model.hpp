#pragma once

#include <string>
#include <string_view>

namespace HTLogic {

using WorkspaceID = long;

enum class NavigateArgKind {
    Direction,
    Invalid,
};

struct NavigateArg {
    NavigateArgKind kind = NavigateArgKind::Invalid;
    std::string direction;
};

NavigateArg parseNavigateArg(std::string_view arg);

} // namespace HTLogic
