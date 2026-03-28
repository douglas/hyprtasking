#include "compat_model.hpp"

#include <cctype>

namespace HTLogic {

std::string normalizeVersion(std::string_view version) {
    while (!version.empty() && std::isspace(static_cast<unsigned char>(version.front())))
        version.remove_prefix(1);

    while (!version.empty() && std::isspace(static_cast<unsigned char>(version.back())))
        version.remove_suffix(1);

    if (version.starts_with('v'))
        version.remove_prefix(1);

    return std::string(version);
}

bool versionMatchesMinor(std::string_view version, std::string_view supported_minor) {
    const auto normalized = normalizeVersion(version);
    if (normalized.empty() || supported_minor.empty())
        return false;

    return std::string_view {normalized}.starts_with(supported_minor);
}

bool versionMatchesExactly(std::string_view left, std::string_view right) {
    return normalizeVersion(left) == normalizeVersion(right);
}

CompatDecision decideCompatSupport(
    bool hashes_match,
    std::string_view runtime_version,
    std::string_view package_version,
    std::string_view supported_minor
) {
    if (!versionMatchesMinor(package_version, supported_minor)) {
        return {
            .supported = false,
            .error = "built against unsupported Hyprland package " + normalizeVersion(package_version),
        };
    }

    if (!versionMatchesMinor(runtime_version, supported_minor)) {
        return {
            .supported = false,
            .error = "running on unsupported Hyprland runtime " + normalizeVersion(runtime_version),
        };
    }

    if (!versionMatchesExactly(runtime_version, package_version)) {
        return {
            .supported = false,
            .error = "installed Hyprland package " + normalizeVersion(package_version) +
                " does not match running Hyprland " + normalizeVersion(runtime_version),
        };
    }

    if (!hashes_match) {
        return {
            .supported = false,
            .error = "mismatched Hyprland headers and compositor build for version " +
                normalizeVersion(runtime_version),
        };
    }

    return {.supported = true};
}

} // namespace HTLogic
