#include "profile.hpp"

#include <filesystem>

#include "build_contract.hpp"
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "../globals.hpp"
#include "../logic/compat_model.hpp"

namespace {

constexpr std::string_view SUPPORTED_HYPRLAND_MINOR = "0.54.";

constexpr HTCompat::HookSpec INPUT_MOUSE_BUTTON_HOOK = {
    .label = "onMouseButton",
    .query = "onMouseButton",
    .fallback_query = "_ZN13CInputManager13onMouseButtonEN8IPointer12SButtonEventE",
};

constexpr HTCompat::HookSpec RENDER_WORKSPACE_HOOK = {
    .label = "renderWorkspace",
    .query = "renderWorkspace",
    .fallback_query = "_ZN13CHyprRenderer15renderWorkspaceEN9Hyprutils6Memory14CSharedPointerI8CMonitorEENS2_I10CWorkspaceEERKNSt6chrono10time_pointINS7_3_V212steady_clockENS7_8durationIlSt5ratioILl1ELl1000000000EEEEEERKNS0_4Math4CBoxE",
};

constexpr HTCompat::HookSpec SHOULD_RENDER_WINDOW_HOOK = {
    .label = "shouldRenderWindow",
    .query = "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
             "haredPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEE",
    .fallback_query = "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
                      "haredPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEE",
};

constexpr HTCompat::HookSpec RENDER_WINDOW_SYMBOL = {
    .label = "renderWindow",
    .query = "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
             "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
             "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
             "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb",
    .fallback_query = "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
                      "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
                      "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
                      "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb",
};

constexpr HTCompat::HookSpec SOLITARY_BLOCKED_HOOK = {
    .label = "isSolitaryBlocked",
    .query = "isSolitaryBlocked",
    .fallback_query = "_ZN8CMonitor17isSolitaryBlockedEb",
};

void* lookup_hook_fallback(std::string_view query) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    const auto address =
        HyprlandAPI::getFunctionAddressFromSignature(PHANDLE, std::string(query));
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    return address;
}

} // namespace

namespace HTCompat {

std::string_view supported_hyprland_minor() {
    return SUPPORTED_HYPRLAND_MINOR;
}

const HookSpec& input_mouse_button_spec() {
    return INPUT_MOUSE_BUTTON_HOOK;
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

HookLookupResult resolve_hook_address(const HookSpec& hook) {
    std::string lookup_error;

    try {
        const auto functions = HyprlandAPI::findFunctionsByName(PHANDLE, std::string(hook.query));
        if (!functions.empty()) {
            return {
                .address = functions[0].address,
                .signature = functions[0].signature,
                .method = "findFunctionsByName",
            };
        }
    } catch (const std::filesystem::filesystem_error& e) {
        lookup_error = "findFunctionsByName failed: " + std::string(e.what());
    } catch (const std::exception& e) {
        lookup_error = "findFunctionsByName failed: " + std::string(e.what());
    }

    const auto fallback = lookup_hook_fallback(hook.fallback_query);
    if (fallback != nullptr) {
        return {
            .address = fallback,
            .signature = std::string(hook.fallback_query),
            .method = "getFunctionAddressFromSignature",
        };
    }

    return {
        .error = lookup_error.empty()
            ? "findFunctionsByName returned no matches and fallback lookup returned null"
            : lookup_error + "; fallback lookup returned null",
    };
}

CompatibilityResult verify_compatibility() {
    const auto version = HyprlandAPI::getHyprlandVersion(PHANDLE);
    const auto reported_version = version.tag.empty() ? std::string_view {version.branch}
                                                      : std::string_view {version.tag};
    const auto compat = HTLogic::decideCompatSupport(
        __hyprland_api_get_hash() == std::string_view {__hyprland_api_get_client_hash()},
        reported_version,
        HT_BUILT_HYPRLAND_PKG_VERSION,
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
             input_mouse_button_spec(),
             render_workspace_spec(),
             should_render_window_spec(),
             render_window_spec(),
             solitary_blocked_spec(),
         }) {
        const auto result = resolve_hook_address(hook);
        if (result.address == nullptr) {
            issues.push_back(
                "missing required hook " + std::string(hook.label) + ": " + result.error
            );
        }
    }

    return issues;
}

} // namespace HTCompat
