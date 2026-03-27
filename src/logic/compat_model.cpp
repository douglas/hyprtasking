#include "compat_model.hpp"

namespace HTLogic {

bool versionMatchesMinor(std::string_view version, std::string_view supported_minor) {
    if (version.empty() || supported_minor.empty())
        return false;

    if (version.starts_with('v'))
        version.remove_prefix(1);

    return version.starts_with(supported_minor);
}

CompatDecision decideCompatSupport(
    bool hashes_match,
    std::string_view build_version,
    std::string_view supported_minor
) {
    if (!hashes_match) {
        return {
            .supported = false,
            .error = "mismatched headers and compositor build",
        };
    }

    if (!versionMatchesMinor(build_version, supported_minor)) {
        return {
            .supported = false,
            .error = "built against unsupported Hyprland " + std::string(build_version),
        };
    }

    return {.supported = true};
}

} // namespace HTLogic
