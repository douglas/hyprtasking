#include "compat_model.hpp"

#include <algorithm>
#include <cctype>
#include <format>

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

bool versionMatchesExactly(std::string_view left, std::string_view right) {
    return normalizeVersion(left) == normalizeVersion(right);
}

bool versionIsSupported(
    std::string_view version,
    std::span<const std::string_view> supported_versions
) {
    const auto normalized = normalizeVersion(version);
    return std::ranges::any_of(supported_versions, [&](const std::string_view candidate) {
        return normalized == normalizeVersion(candidate);
    });
}

std::string supportedVersionsList(std::span<const std::string_view> supported_versions) {
    std::string result;

    for (size_t i = 0; i < supported_versions.size(); ++i) {
        if (i > 0)
            result += ", ";

        result += normalizeVersion(supported_versions[i]);
    }

    return result;
}

CompatDecision decideCompatSupport(
    bool hashes_match,
    std::string_view runtime_version,
    std::string_view package_version,
    std::span<const std::string_view> supported_versions
) {
    const auto normalized_package = normalizeVersion(package_version);
    const auto normalized_runtime = normalizeVersion(runtime_version);
    const auto supported_versions_list = supportedVersionsList(supported_versions);

    if (!versionIsSupported(package_version, supported_versions)) {
        return {
            .supported = false,
            .status = CompatStatus::UnsupportedPackageVersion,
            .error = std::format(
                "built against unsupported Hyprland package {} (supported: {})",
                normalized_package,
                supported_versions_list
            ),
        };
    }

    if (!versionIsSupported(runtime_version, supported_versions)) {
        return {
            .supported = false,
            .status = CompatStatus::UnsupportedRuntimeVersion,
            .error = std::format(
                "running on unsupported Hyprland runtime {} (supported: {})",
                normalized_runtime,
                supported_versions_list
            ),
        };
    }

    if (!versionMatchesExactly(runtime_version, package_version)) {
        return {
            .supported = false,
            .status = CompatStatus::RuntimePackageMismatch,
            .error = "installed Hyprland package " + normalized_package +
                " does not match running Hyprland " + normalized_runtime,
        };
    }

    if (!hashes_match) {
        return {
            .supported = false,
            .status = CompatStatus::HeaderRuntimeMismatch,
            .error = "mismatched Hyprland headers and compositor build for version " + normalized_runtime,
        };
    }

    return {.supported = true, .status = CompatStatus::Supported};
}

} // namespace HTLogic
