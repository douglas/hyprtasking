#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace HTCompat {

struct HookSpec {
    std::string_view label;
    std::string_view query;
};

struct CompatibilityResult {
    bool        supported = false;
    std::string error;
};

std::string_view supported_hyprland_minor();
const HookSpec& render_workspace_spec();
const HookSpec& should_render_window_spec();
const HookSpec& render_window_spec();
const HookSpec& solitary_blocked_spec();
CompatibilityResult verify_compatibility();
std::vector<std::string> audit_compatibility_issues();

} // namespace HTCompat
