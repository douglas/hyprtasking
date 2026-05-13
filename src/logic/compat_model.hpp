#pragma once

#include <span>
#include <string>
#include <string_view>

namespace HTLogic {

enum class CompatStatus {
    Supported,
    UnsupportedPackageVersion,
    UnsupportedRuntimeVersion,
    RuntimePackageMismatch,
    HeaderRuntimeMismatch,
};

struct CompatDecision {
    bool         supported = false;
    CompatStatus status = CompatStatus::Supported;
    std::string  error;
};

std::string normalizeVersion(std::string_view version);
bool versionMatchesExactly(std::string_view left, std::string_view right);
bool versionIsSupported(
    std::string_view version,
    std::span<const std::string_view> supported_versions
);
std::string supportedVersionsList(std::span<const std::string_view> supported_versions);
CompatDecision decideCompatSupport(
    bool hashes_match,
    std::string_view runtime_version,
    std::string_view package_version,
    std::span<const std::string_view> supported_versions
);

} // namespace HTLogic
