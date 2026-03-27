#pragma once

#include <string>
#include <string_view>

namespace HTLogic {

struct CompatDecision {
    bool        supported = false;
    std::string error;
};

bool versionMatchesMinor(std::string_view version, std::string_view supported_minor);
CompatDecision decideCompatSupport(
    bool hashes_match,
    std::string_view build_version,
    std::string_view supported_minor
);

} // namespace HTLogic
