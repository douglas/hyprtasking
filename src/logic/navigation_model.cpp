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

} // namespace HTLogic
