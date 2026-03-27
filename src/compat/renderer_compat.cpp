#include "renderer_compat.hpp"

#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>

#include "../globals.hpp"
#include "../logic/geometry_model.hpp"
#include "../overview.hpp"
#include "../pass/pass_element.hpp"
#include "../plugin/guards.hpp"
#include "../types.hpp"
#include "profile.hpp"

namespace {

const std::string CLEAR_PASS_ELEMENT_NAME = "CClearPassElement";

uint32_t solitaryBlockedOriginal(void* thisptr, bool full) {
    return (*(origIsSolitaryBlocked)is_solitary_blocked_hook->m_original)(thisptr, full);
}

void hookRenderWorkspace(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    try {
        if (ht_manager == nullptr) {
            HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
            return;
        }
        if (!ht_manager->runtime_enabled()) {
            HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
            return;
        }

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view != nullptr && (view->navigating || ht_manager->has_active_view())) {
            view->layout->render();
            return;
        }

        HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
    } catch (const std::exception& e) {
        Log::logger->log(Log::ERR, "[Hyprtasking] hook_render_workspace failed: {}", e.what());
        HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
    } catch (...) {
        Log::logger->log(
            Log::ERR,
            "[Hyprtasking] hook_render_workspace failed with unknown exception"
        );
        HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
    }
}

bool hookShouldRenderWindow(void* thisptr, PHLWINDOW window, PHLMONITOR monitor) {
    const bool original_result = HTCompat::should_render_window_original(thisptr, window, monitor);
    return HTPlugin::guardedValue("hook_should_render_window", original_result, [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled() || !ht_manager->has_active_view())
            return original_result;

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view == nullptr)
            return original_result;

        return view->layout->should_render_window(window);
    });
}

uint32_t hookIsSolitaryBlocked(void* thisptr, bool full) {
    return HTPlugin::guardedValue(
        "hook_is_solitary_blocked",
        solitaryBlockedOriginal(thisptr, full),
        [&] {
            if (ht_manager == nullptr)
                return solitaryBlockedOriginal(thisptr, full);
            if (!ht_manager->runtime_enabled())
                return solitaryBlockedOriginal(thisptr, full);

            const PHTVIEW view = ht_manager->get_view_from_cursor();
            if (view == nullptr)
                return solitaryBlockedOriginal(thisptr, full);

            if (view->active || view->navigating)
                return static_cast<uint32_t>(CMonitor::SC_UNKNOWN);

            return solitaryBlockedOriginal(thisptr, full);
        }
    );
}

void unhook(CFunctionHook*& hook, const char* name) {
    if (hook == nullptr)
        return;

    if (!hook->unhook())
        Log::logger->log(Log::WARN, "[Hyprtasking] Failed to unhook {}", name);
    hook = nullptr;
}

}

namespace HTCompat {

void initializeRendererHooks() {
    bool success = true;

    static const auto render_workspace_functions = HyprlandAPI::findFunctionsByName(
        PHANDLE,
        std::string(render_workspace_spec().query)
    );
    if (render_workspace_functions.empty())
        fail_exit("No renderWorkspace!");
    render_workspace_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        render_workspace_functions[0].address,
        (void*)hookRenderWorkspace
    );
    Log::logger->log(
        LOG,
        "[Hyprtasking] Attempting hook {}",
        render_workspace_functions[0].signature
    );
    success = render_workspace_hook->hook();

    static const auto should_render_window_functions = HyprlandAPI::findFunctionsByName(
        PHANDLE,
        std::string(should_render_window_spec().query)
    );
    if (should_render_window_functions.empty())
        fail_exit("No shouldRenderWindow");
    should_render_window_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        should_render_window_functions[0].address,
        (void*)hookShouldRenderWindow
    );
    Log::logger->log(
        LOG,
        "[Hyprtasking] Attempting hook {}",
        should_render_window_functions[0].signature
    );
    success = should_render_window_hook->hook() && success;

    static const auto render_window_functions = HyprlandAPI::findFunctionsByName(
        PHANDLE,
        std::string(render_window_spec().query)
    );
    if (render_window_functions.empty())
        fail_exit("No renderWindow");
    render_window = render_window_functions[0].address;

    static const auto solitary_blocked_functions = HyprlandAPI::findFunctionsByName(
        PHANDLE,
        std::string(solitary_blocked_spec().query)
    );
    if (solitary_blocked_functions.empty())
        fail_exit("No isSolitaryBlocked");

    is_solitary_blocked_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        solitary_blocked_functions[0].address,
        (void*)hookIsSolitaryBlocked
    );
    Log::logger->log(
        LOG,
        "[Hyprtasking] Attempting hook {}",
        solitary_blocked_functions[0].signature
    );
    success = is_solitary_blocked_hook->hook() && success;

    if (!success) {
        shutdownRendererHooks();
        fail_exit("Failed initializing hooks");
    }
}

void shutdownRendererHooks() {
    unhook(render_workspace_hook, "renderWorkspace");
    unhook(should_render_window_hook, "shouldRenderWindow");
    unhook(is_solitary_blocked_hook, "isSolitaryBlocked");
    render_window = nullptr;
}

bool should_render_window_original(void* renderer, PHLWINDOW window, PHLMONITOR monitor) {
    if (renderer == nullptr || should_render_window_hook == nullptr || window == nullptr
        || monitor == nullptr)
        return false;

    return ((should_render_window_t)(should_render_window_hook->m_original))(
        renderer,
        window,
        monitor
    );
}

void render_workspace_original(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    if (thisptr == nullptr || render_workspace_hook == nullptr)
        return;

    ((render_workspace_t)(render_workspace_hook->m_original))(
        thisptr,
        monitor,
        workspace,
        now,
        geometry
    );
}

void render_window_original(
    void* thisptr,
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    eRenderPassMode mode,
    bool ignore_position,
    bool standalone
) {
    if (thisptr == nullptr || render_window == nullptr || window == nullptr || monitor == nullptr)
        return;

    ((render_window_t)render_window)(
        thisptr,
        window,
        monitor,
        time,
        decorate,
        mode,
        ignore_position,
        standalone
    );
}

Vector2D monitor_position(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return {};

    return monitor->m_position;
}

MONITORID monitor_id(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return MONITOR_INVALID;

    return monitor->m_id;
}

PHLWORKSPACE active_monitor_workspace(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return nullptr;

    return monitor->m_activeWorkspace;
}

PHLMONITOR workspace_monitor(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return nullptr;

    return workspace->m_monitor.lock();
}

bool workspace_render_visible(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return false;

    return workspace->m_visible;
}

bool workspace_is_special(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return false;

    return workspace->m_isSpecialWorkspace;
}

WORKSPACEID workspace_id(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return WORKSPACE_INVALID;

    return workspace->m_id;
}

PHLWORKSPACE window_workspace(PHLWINDOW window) {
    if (window == nullptr)
        return nullptr;

    return window->m_workspace;
}

PHLMONITOR window_monitor(PHLWINDOW window) {
    if (window == nullptr)
        return nullptr;

    return window->m_monitor.lock();
}

void warp_workspace_render_offset(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return;

    workspace->m_renderOffset->warp();
}

bool activate_monitor_workspace(PHLMONITOR monitor, PHLWORKSPACE workspace) {
    return restore_monitor_workspace(monitor, workspace, true);
}

bool restore_monitor_workspace(PHLMONITOR monitor, PHLWORKSPACE workspace, bool use_change_workspace) {
    if (monitor == nullptr || workspace == nullptr)
        return false;

    if (use_change_workspace)
        monitor->changeWorkspace(workspace, true);
    else
        monitor->m_activeWorkspace = workspace;

    return true;
}

void set_workspace_render_visibility(PHLWORKSPACE workspace, bool visible) {
    if (workspace == nullptr)
        return;

    g_pDesktopAnimationManager->startAnimation(
        workspace,
        visible ? CDesktopAnimationManager::ANIMATION_TYPE_IN
                : CDesktopAnimationManager::ANIMATION_TYPE_OUT,
        false,
        true
    );
    workspace->m_visible = visible;
}

PHLWORKSPACE resolve_workspace_target(
    PHLMONITOR monitor,
    WORKSPACEID workspace_id,
    bool create_if_missing
) {
    if (monitor == nullptr || workspace_id == WORKSPACE_INVALID)
        return nullptr;

    PHLWORKSPACE workspace = g_pCompositor->getWorkspaceByID(workspace_id);
    if (workspace == nullptr && create_if_missing)
        workspace = g_pCompositor->createNewWorkspace(workspace_id, HTCompat::monitor_id(monitor));

    return workspace;
}

bool warp_pointer(const Vector2D& position) {
    if (!HTLogic::isFinitePoint(position.x, position.y) || !g_pPointerManager)
        return false;

    g_pPointerManager->warpTo(position);
    return true;
}

void begin_overview_render_pass() {
    if (!g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<HTPassElement>());
}

void remove_clear_passes() {
    if (!g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->m_renderPass.removeAllOfType(CLEAR_PASS_ELEMENT_NAME);
}

void finalize_overview_render_pass() {
    // begin_overview_render_pass adds the simplification guard up front.
}

}
