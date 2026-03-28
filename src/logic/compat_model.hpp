#pragma once

#include <string>
#include <string_view>

namespace HTLogic {

struct CompatDecision {
    bool        supported = false;
    std::string error;
};

std::string normalizeVersion(std::string_view version);
bool versionMatchesMinor(std::string_view version, std::string_view supported_minor);
bool versionMatchesExactly(std::string_view left, std::string_view right);
CompatDecision decideCompatSupport(
    bool hashes_match,
    std::string_view runtime_version,
    std::string_view package_version,
    std::string_view supported_minor
);

} // namespace HTLogic
