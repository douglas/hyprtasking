#include "renderer_compat.hpp"

// Hyprland exposes CHyprRenderer::m_renderPass publicly, but CRenderPass does not expose
// pass-element inspection or reordering. We only need privileged access to Pass.hpp here so we
// can prune duplicate clear passes without perturbing render order.
#define private public
#include <hyprland/src/render/pass/Pass.hpp>
#undef private

#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/ClearPassElement.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>

#include "../globals.hpp"
#include "../overview.hpp"
#include "../pass/pass_element.hpp"
#include "../plugin/guards.hpp"
#include "../types.hpp"

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
        if (ht_manager == nullptr || !ht_manager->has_active_view())
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

    static const auto render_workspace_functions =
        HyprlandAPI::findFunctionsByName(PHANDLE, "renderWorkspace");
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
        "_ZN13CHyprRenderer18shouldRenderWindowEN9Hyprutils6Memory14CS"
        "haredPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEE"
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
        "_ZN13CHyprRenderer12renderWindowEN9Hyprutils6Memory14CSha"
        "redPointerIN7Desktop4View7CWindowEEENS2_I8CMonitorEERKNSt"
        "6chrono10time_pointINS9_3_V212steady_clockENS9_8durationI"
        "lSt5ratioILl1ELl1000000000EEEEEEb15eRenderPassModebb"
    );
    if (render_window_functions.empty())
        fail_exit("No renderWindow");
    render_window = render_window_functions[0].address;

    static const auto solitary_blocked_functions =
        HyprlandAPI::findFunctionsByName(PHANDLE, "isSolitaryBlocked");
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

void add_clear_pass() {
    if (!g_pHyprRenderer.get())
        return;

    CClearPassElement::SClearData data;
    data.color = CHyprColor {0};
    g_pHyprRenderer->m_renderPass.add(makeUnique<CClearPassElement>(data));
}

void finalize_overview_render_pass() {
    if (!g_pHyprRenderer.get())
        return;

    bool first = true;
    std::erase_if(g_pHyprRenderer->m_renderPass.m_passElements, [&first](const auto& e) {
        const bool remove = e->element->passName() == CLEAR_PASS_ELEMENT_NAME && !first;
        first = false;
        return remove;
    });
    g_pHyprRenderer->m_renderPass.add(makeUnique<HTPassElement>());
}

}
