#include "renderer_hooks.hpp"

#include <string>

#include <hyprland/src/render/Renderer.hpp>

#include "../globals.hpp"
#include "../overview.hpp"

namespace {

struct SRendererHookState {
    CFunctionHook* should_render_window_hook = nullptr;
    void*          render_workspace          = nullptr;
    void*          render_window             = nullptr;
    bool           rendering_overview        = false;
};

SRendererHookState& hookState() {
    static SRendererHookState state;
    return state;
}

SFunctionMatch findFunctionMatch(
    const std::string& label,
    const std::string& query,
    const std::string& signature = ""
) {
    const auto matches = HyprlandAPI::findFunctionsByName(PHANDLE, query);
    if (matches.empty())
        fail_exit("No {} for query {}", label, query);

    if (signature.empty())
        return matches[0];

    for (const auto& match : matches) {
        if (match.signature == signature)
            return match;
    }

    Log::logger->log(
        LOG,
        "[Hyprtasking] No exact {} match for {}. {} candidate(s) returned for query {}",
        label,
        signature,
        matches.size(),
        query
    );
    for (const auto& match : matches) {
        Log::logger->log(
            LOG,
            "[Hyprtasking] Candidate {} hook signature: {}",
            label,
            match.signature
        );
    }

    fail_exit("No exact {} match for {}", label, signature);
    __builtin_unreachable();
}

bool hookShouldRenderWindow(void* thisptr, PHLWINDOW window, PHLMONITOR monitor) {
    const auto& state = hookState();
    const bool  original_result =
        ((should_render_window_t)(state.should_render_window_hook->m_original))(thisptr, window, monitor);

    try {
        if (ht_manager == nullptr || !ht_manager->has_active_view())
            return original_result;

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view == nullptr)
            return original_result;

        return view->layout->should_render_window(window);
    } catch (const std::exception& e) {
        Log::logger->log(Log::ERR, "[Hyprtasking] hookShouldRenderWindow failed: {}", e.what());
        return original_result;
    } catch (...) {
        Log::logger->log(
            Log::ERR,
            "[Hyprtasking] hookShouldRenderWindow failed with unknown exception"
        );
        return original_result;
    }
}

}

namespace HTCompat {

void initializeRendererHooks() {
    auto& state = hookState();

    static const auto render_workspace_match = findFunctionMatch(
        "renderWorkspace",
        "renderWorkspace",
        "_ZN13CHyprRenderer15renderWorkspaceEN9Hyprutils6Memory14CSharedPointerI8CMon"
        "itorEENS2_I10CWorkspaceEERKNSt6chrono10time_pointINS7_3_V212steady_clockENS7"
        "_8durationIlSt5ratioILl1ELl1000000000EEEEEERKNS0_4Math4CBoxE"
    );
    state.render_workspace = render_workspace_match.address;

    static const auto should_render_window_match = findFunctionMatch(
        "shouldRenderWindow",
        "shouldRenderWindow",
        "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CSharedPointerIN7"
        "Desktop4View7CWindowEEENS2_I8CMonitorEE"
    );
    state.should_render_window_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        should_render_window_match.address,
        (void*)hookShouldRenderWindow
    );
    Log::logger->log(
        LOG,
        "[Hyprtasking] Attempting hook {}",
        should_render_window_match.signature
    );
    if (state.should_render_window_hook == nullptr || !state.should_render_window_hook->hook())
        fail_exit("Failed initializing shouldRenderWindow hook");

    static const auto render_window_match = findFunctionMatch(
        "renderWindow",
        "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
        "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
        "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
        "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb",
        "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
        "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
        "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
        "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb"
    );
    state.render_window = render_window_match.address;
}

void shutdownRendererHooks() {
    auto& state = hookState();

    if (state.should_render_window_hook != nullptr) {
        if (!state.should_render_window_hook->unhook()) {
            Log::logger->log(Log::ERR, "[Hyprtasking] Failed to unhook shouldRenderWindow");
        }
        state.should_render_window_hook = nullptr;
    }

    state.render_workspace   = nullptr;
    state.render_window      = nullptr;
    state.rendering_overview = false;
}

bool callOriginalShouldRenderWindow(PHLWINDOW window, PHLMONITOR monitor) {
    if (window == nullptr || monitor == nullptr)
        return false;

    const auto& state = hookState();
    if (state.should_render_window_hook == nullptr)
        return g_pHyprRenderer->shouldRenderWindow(window, monitor);

    return ((should_render_window_t)(state.should_render_window_hook->m_original))(
        g_pHyprRenderer.get(),
        window,
        monitor
    );
}

void renderWorkspaceOriginal(
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    const auto& state = hookState();
    if (state.render_workspace == nullptr || monitor == nullptr)
        return;

    ((render_workspace_t)state.render_workspace)(g_pHyprRenderer.get(), monitor, workspace, now, geometry);
}

void renderWindowOriginal(
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    eRenderPassMode mode,
    bool ignorePosition,
    bool standalone
) {
    const auto& state = hookState();
    if (state.render_window == nullptr || window == nullptr || monitor == nullptr)
        return;

    ((render_window_t)state.render_window)(
        g_pHyprRenderer.get(),
        window,
        monitor,
        time,
        decorate,
        mode,
        ignorePosition,
        standalone
    );
}

bool beginOverviewRender() {
    auto& state = hookState();
    if (state.rendering_overview)
        return false;

    state.rendering_overview = true;
    return true;
}

void endOverviewRender() {
    hookState().rendering_overview = false;
}

}
