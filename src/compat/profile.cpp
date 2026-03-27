#include "profile.hpp"

#include <hyprland/src/plugins/PluginAPI.hpp>

#include "../globals.hpp"
#include "../logic/compat_model.hpp"

namespace {

constexpr std::string_view SUPPORTED_HYPRLAND_MINOR = "0.54.";

constexpr HTCompat::HookSpec RENDER_WORKSPACE_HOOK = {
    .label = "renderWorkspace",
    .query = "renderWorkspace",
};

constexpr HTCompat::HookSpec SHOULD_RENDER_WINDOW_HOOK = {
    .label = "shouldRenderWindow",
    .query = "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
             "haredPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEE",
};

constexpr HTCompat::HookSpec RENDER_WINDOW_SYMBOL = {
    .label = "renderWindow",
    .query = "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
             "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
             "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
             "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb",
};

constexpr HTCompat::HookSpec SOLITARY_BLOCKED_HOOK = {
    .label = "isSolitaryBlocked",
    .query = "isSolitaryBlocked",
};

bool hookAvailable(const HTCompat::HookSpec& hook) {
    return !HyprlandAPI::findFunctionsByName(PHANDLE, std::string(hook.query)).empty();
}

} // namespace

namespace HTCompat {

std::string_view supported_hyprland_minor() {
    return SUPPORTED_HYPRLAND_MINOR;
}

const HookSpec& render_workspace_spec() {
    return RENDER_WORKSPACE_HOOK;
}

const HookSpec& should_render_window_spec() {
    return SHOULD_RENDER_WINDOW_HOOK;
}

const HookSpec& render_window_spec() {
    return RENDER_WINDOW_SYMBOL;
}

const HookSpec& solitary_blocked_spec() {
    return SOLITARY_BLOCKED_HOOK;
}

CompatibilityResult verify_compatibility() {
    const auto version = HyprlandAPI::getHyprlandVersion(PHANDLE);
    const auto reported_version = version.tag.empty() ? std::string_view {version.branch}
                                                      : std::string_view {version.tag};
    const auto compat = HTLogic::decideCompatSupport(
        __hyprland_api_get_hash() == std::string_view {__hyprland_api_get_client_hash()},
        reported_version,
        supported_hyprland_minor()
    );
    if (!compat.supported)
        return {.supported = false, .error = compat.error};

    const auto issues = audit_compatibility_issues();
    if (!issues.empty())
        return {.supported = false, .error = issues.front()};

    return {.supported = true};
}

std::vector<std::string> audit_compatibility_issues() {
    std::vector<std::string> issues;

    for (const auto& hook : {
             render_workspace_spec(),
             should_render_window_spec(),
             render_window_spec(),
             solitary_blocked_spec(),
         }) {
        if (!hookAvailable(hook))
            issues.push_back("missing required hook " + std::string(hook.label));
    }

    return issues;
}

} // namespace HTCompat
