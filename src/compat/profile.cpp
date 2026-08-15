#include "profile.hpp"

#include <array>
#include <format>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "../globals.hpp"
#include "../logic/compat_model.hpp"
#include "build_contract.hpp"

namespace {

constexpr std::array<std::string_view, 4> SUPPORTED_HYPRLAND_VERSIONS = {
    "0.54.3",
    "0.55",
    "0.56.0",
    "0.56.2",
};

constexpr HTCompat::HookSpec INPUT_MOUSE_BUTTON_HOOK = {
    .label = "onMouseButton",
    .query = "onMouseButton",
#if HT_HYPRLAND_GE_0_55
    .signature =
        "_ZN13CInputManager13onMouseButtonEN8IPointer12SButtonEventEN9Hyprutils6Memory"
        "14CSharedPointerIS0_EE",
#else
    .signature = "_ZN13CInputManager13onMouseButtonEN8IPointer12SButtonEventE",
#endif
};

constexpr HTCompat::HookSpec RENDER_WORKSPACE_HOOK = {
    .label = "renderWorkspace",
    .query = "renderWorkspace",
#if HT_HYPRLAND_GE_0_55
    .signature =
        "_ZN6Render13IHyprRenderer15renderWorkspaceEN9Hyprutils6Memory14CSharedPointerI8"
        "CMonitorEENS3_I10CWorkspaceEERKNSt6chrono10time_pointINS8_3_V212steady_clockENS"
        "8_8durationIlSt5ratioILl1ELl1000000000EEEEEERKNS1_4Math4CBoxE",
#else
    .signature =
        "_ZN13CHyprRenderer15renderWorkspaceEN9Hyprutils6Memory14CSharedPointerI8CMonitorEENS2_I10CWorkspaceEERKNSt6chrono10time_pointINS7_3_V212steady_clockENS7_8durationIlSt5ratioILl1ELl1000000000EEEEEERKNS0_4Math4CBoxE",
#endif
};

constexpr HTCompat::HookSpec SHOULD_RENDER_WINDOW_HOOK = {
    .label = "shouldRenderWindow",
#if HT_HYPRLAND_GE_0_55
    .query =
        "_ZN6Render13IHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
        "haredPointerIN7Desktop4View7CWindowEEENS3_I8CMonitorEE",
    .signature =
        "_ZN6Render13IHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
        "haredPointerIN7Desktop4View7CWindowEEENS3_I8CMonitorEE",
#else
    .query =
        "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
        "haredPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEE",
    .signature =
        "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
        "haredPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEE",
#endif
};

constexpr HTCompat::HookSpec RENDER_WINDOW_SYMBOL = {
    .label = "renderWindow",
#if HT_HYPRLAND_GE_0_55
    .query =
        "_ZN6Render13IHyprRenderer12renderWindowEN9Hyprutils6Memory14CSharedPointer"
        "IN7Desktop4View7CWindowEEENS3_I8CMonitorEERKNSt6chrono10time_pointINSA_3"
        "_V212steady_clockENSA_8durationIlSt5ratioILl1ELl1000000000EEEEEEbNS_15"
        "eRenderPassModeEbb",
    .signature =
        "_ZN6Render13IHyprRenderer12renderWindowEN9Hyprutils6Memory14CSharedPointer"
        "IN7Desktop4View7CWindowEEENS3_I8CMonitorEERKNSt6chrono10time_pointINSA_3"
        "_V212steady_clockENSA_8durationIlSt5ratioILl1ELl1000000000EEEEEEbNS_15"
        "eRenderPassModeEbb",
#else
    .query =
        "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
        "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
        "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
        "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb",
    .signature =
        "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
        "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
        "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
        "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb",
#endif
};

constexpr HTCompat::HookSpec SOLITARY_BLOCKED_HOOK = {
    .label = "isSolitaryBlocked",
    .query = "isSolitaryBlocked",
    .signature = "_ZN8CMonitor17isSolitaryBlockedEb",
};

} // namespace

namespace HTCompat {

std::span<const std::string_view> supported_hyprland_versions() {
    return SUPPORTED_HYPRLAND_VERSIONS;
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
        for (const auto& function : functions) {
            if (function.address != nullptr && function.signature == hook.signature) {
                return {
                    .address = function.address,
                    .signature = function.signature,
                    .method = "findFunctionsByName",
                };
            }
        }
        if (!functions.empty())
            lookup_error = std::format(
                "findFunctionsByName returned {} match(es), but none matched expected signature {}",
                functions.size(),
                hook.signature
            );
    } catch (const std::exception& e) {
        lookup_error = "findFunctionsByName failed: " + std::string(e.what());
    }

    return {
        .error = lookup_error.empty()
            ? std::format(
                  "findFunctionsByName returned no matches for expected signature {}",
                  hook.signature
              )
            : lookup_error,
    };
}

HookInstallResult
install_function_hook(CFunctionHook*& hook, const HookSpec& spec, void* destination) {
    if (hook != nullptr)
        return {.error = std::format("{} hook is already installed", spec.label)};
    if (destination == nullptr)
        return {.error = std::format("{} hook destination is null", spec.label)};

    const auto lookup = resolve_hook_address(spec);
    if (lookup.address == nullptr)
        return {.error = std::format("No {}: {}", spec.label, lookup.error)};

    hook = HyprlandAPI::createFunctionHook(PHANDLE, lookup.address, destination);
    if (hook == nullptr)
        return {.error = std::format("failed creating {} hook", spec.label)};

    Log::logger
        ->log(LOG, "[Hyprtasking] Attempting hook {} via {}", lookup.signature, lookup.method);

    if (!hook->hook()) {
        remove_function_hook(hook, spec.label);
        return {.error = std::format("failed initializing {} hook", spec.label)};
    }

    if (hook->m_original == nullptr) {
        remove_function_hook(hook, spec.label);
        return {
            .error = std::format("{} hook installed without original call-through", spec.label)
        };
    }

    return {
        .installed = true,
        .signature = lookup.signature,
        .method = lookup.method,
    };
}

bool remove_function_hook(CFunctionHook*& hook, std::string_view label) {
    if (hook == nullptr)
        return true;

    const bool removed = HyprlandAPI::removeFunctionHook(PHANDLE, hook);
    if (!removed)
        Log::logger->log(Log::WARN, "[Hyprtasking] Failed to remove hook {}", label);

    hook = nullptr;
    return removed;
}

bool function_hook_original_ready(CFunctionHook* hook) {
    return hook != nullptr && hook->m_original != nullptr;
}

CompatibilityResult verify_compatibility() {
#if !HT_FUNCTION_HOOKS_SUPPORTED_ARCH
    return {
        .supported = false,
        .error = "Hyprtasking function hooks are supported only on x86_64 builds"
    };
#endif

    const auto version = HyprlandAPI::getHyprlandVersion(PHANDLE);
    const auto reported_version =
        version.tag.empty() ? std::string_view {version.branch} : std::string_view {version.tag};
    const auto compat = HTLogic::decideCompatSupport(
        __hyprland_api_get_hash() == std::string_view {__hyprland_api_get_client_hash()},
        reported_version,
        HT_BUILT_HYPRLAND_PKG_VERSION,
        supported_hyprland_versions()
    );
    if (!compat.supported)
        return {.supported = false, .error = compat.error};

    return {.supported = true};
}

} // namespace HTCompat
