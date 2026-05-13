#include "navigation_model.hpp"

#include <cctype>

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

    return {.kind = NavigateArgKind::Invalid};
}

} // namespace HTLogic
