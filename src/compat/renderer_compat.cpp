#include "renderer_compat.hpp"

#include <format>
#include <string_view>

#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "../globals.hpp"
#include "../logic/geometry_model.hpp"
#include "../overview.hpp"
#include "../pass/pass_element.hpp"
#include "../plugin/guards.hpp"
#include "../types.hpp"
#include "profile.hpp"

namespace {

const std::string CLEAR_PASS_ELEMENT_NAME = "CClearPassElement";
thread_local int g_overview_render_scope_depth = 0;
thread_local int g_overview_render_pass_depth = 0;

using Hyprutils::Utils::CScopeGuard;

void disable_runtime_for_render_failure(std::string_view source, std::string_view reason) {
    Log::logger->log(Log::ERR, "[Hyprtasking] {}: {}", source, reason);
    if (ht_manager != nullptr)
        ht_manager->disable_runtime(source, reason);
}

bool begin_overview_render_scope(PHLMONITOR monitor, PHLWORKSPACE workspace) {
    if (g_overview_render_scope_depth > 0) {
        disable_runtime_for_render_failure(
            "hook_render_workspace",
            std::format(
                "overview render re-entry detected for monitor {} workspace {}",
                HTCompat::monitor_id(monitor),
                HTCompat::workspace_id(workspace)
            )
        );
        return false;
    }

    g_overview_render_scope_depth++;
    return true;
}

void end_overview_render_scope() {
    if (g_overview_render_scope_depth <= 0) {
        Log::logger->log(
            Log::WARN,
            "[Hyprtasking] finalize_overview_render_scope called with no active scope"
        );
        g_overview_render_scope_depth = 0;
        g_overview_render_pass_depth = 0;
        return;
    }

    g_overview_render_scope_depth--;
    if (g_overview_render_scope_depth == 0)
        g_overview_render_pass_depth = 0;
}

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
            if (!begin_overview_render_scope(monitor, workspace)) {
                HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry);
                return;
            }

            CScopeGuard render_scope([] { end_overview_render_scope(); });
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

    const auto render_workspace = resolve_hook_address(render_workspace_spec());
    if (render_workspace.address == nullptr)
        fail_exit("No {}: {}", render_workspace_spec().label, render_workspace.error);
    render_workspace_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        render_workspace.address,
        (void*)hookRenderWorkspace
    );
    Log::logger->log(
        LOG,
        "[Hyprtasking] Attempting hook {} via {}",
        render_workspace.signature,
        render_workspace.method
    );
    success = render_workspace_hook->hook();

    const auto should_render_window = resolve_hook_address(should_render_window_spec());
    if (should_render_window.address == nullptr)
        fail_exit(
            "No {}: {}",
            should_render_window_spec().label,
            should_render_window.error
        );
    should_render_window_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        should_render_window.address,
        (void*)hookShouldRenderWindow
    );
    Log::logger->log(
        LOG,
        "[Hyprtasking] Attempting hook {} via {}",
        should_render_window.signature,
        should_render_window.method
    );
    success = should_render_window_hook->hook() && success;

    const auto render_window_symbol = resolve_hook_address(render_window_spec());
    if (render_window_symbol.address == nullptr)
        fail_exit("No {}: {}", render_window_spec().label, render_window_symbol.error);
    render_window = render_window_symbol.address;
    Log::logger->log(
        LOG,
        "[Hyprtasking] Resolved {} via {}",
        render_window_symbol.signature,
        render_window_symbol.method
    );

    const auto solitary_blocked = resolve_hook_address(solitary_blocked_spec());
    if (solitary_blocked.address == nullptr)
        fail_exit("No {}: {}", solitary_blocked_spec().label, solitary_blocked.error);

    is_solitary_blocked_hook = HyprlandAPI::createFunctionHook(
        PHANDLE,
        solitary_blocked.address,
        (void*)hookIsSolitaryBlocked
    );
    Log::logger->log(
        LOG,
        "[Hyprtasking] Attempting hook {} via {}",
        solitary_blocked.signature,
        solitary_blocked.method
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

bool should_render_window_original(PHLWINDOW window, PHLMONITOR monitor) {
    return should_render_window_original(g_pHyprRenderer.get(), window, monitor);
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
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    render_workspace_original(g_pHyprRenderer.get(), monitor, workspace, now, geometry);
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
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    eRenderPassMode mode,
    bool ignore_position,
    bool standalone
) {
    render_window_original(
        g_pHyprRenderer.get(),
        window,
        monitor,
        time,
        decorate,
        mode,
        ignore_position,
        standalone
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

float monitor_scale(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return 0.F;

    return monitor->m_scale;
}

Vector2D monitor_transformed_size(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return {};

    return monitor->m_transformedSize;
}

Vector2D monitor_pixel_size(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return {};

    return monitor->m_pixelSize;
}

int monitor_transform(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return 0;

    return monitor->m_transform;
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

PHLWORKSPACE workspace_by_id(WORKSPACEID workspace_id) {
    if (workspace_id == WORKSPACE_INVALID || !g_pCompositor)
        return nullptr;

    return g_pCompositor->getWorkspaceByID(workspace_id);
}

std::vector<PHLWORKSPACE> compositor_workspaces() {
    if (!g_pCompositor)
        return {};

    return g_pCompositor->getWorkspacesCopy();
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

Vector2D window_real_position(PHLWINDOW window) {
    if (window == nullptr)
        return {};

    return window->m_realPosition->value();
}

Vector2D window_real_position_goal(PHLWINDOW window) {
    if (window == nullptr)
        return {};

    return window->m_realPosition->goal();
}

Vector2D window_real_size(PHLWINDOW window) {
    if (window == nullptr)
        return {};

    return window->m_realSize->value();
}

void set_window_real_position(PHLWINDOW window, const Vector2D& position) {
    if (window == nullptr)
        return;

    window->m_realPosition->setValueAndWarp(position);
}

void set_window_real_position_goal(PHLWINDOW window, const Vector2D& position) {
    if (window == nullptr)
        return;

    *window->m_realPosition = position;
}

void reset_window_workspace_move_alpha(PHLWINDOW window) {
    if (window == nullptr)
        return;

    window->m_movingToWorkspaceAlpha->setValueAndWarp(1.0);
    window->m_movingFromWorkspaceAlpha->setValueAndWarp(1.0);
}

void warp_workspace_render_offset(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return;

    workspace->m_renderOffset->warp();
}

bool activate_monitor_workspace_user(PHLMONITOR monitor, PHLWORKSPACE workspace) {
    if (monitor == nullptr || workspace == nullptr)
        return false;

    monitor->changeWorkspace(workspace, false);
    return true;
}

bool activate_monitor_workspace_internal(PHLMONITOR monitor, PHLWORKSPACE workspace) {
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

    PHLWORKSPACE workspace = HTCompat::workspace_by_id(workspace_id);
    if (workspace == nullptr && create_if_missing)
        workspace = g_pCompositor->createNewWorkspace(workspace_id, HTCompat::monitor_id(monitor));

    return workspace;
}

bool move_workspace_to_monitor(PHLWORKSPACE workspace, PHLMONITOR monitor, bool no_warp_cursor) {
    if (workspace == nullptr || monitor == nullptr || !g_pCompositor)
        return false;

    g_pCompositor->moveWorkspaceToMonitor(workspace, monitor, no_warp_cursor);
    return true;
}

bool warp_pointer(const Vector2D& position) {
    if (!HTLogic::isFinitePoint(position.x, position.y) || !g_pPointerManager)
        return false;

    g_pPointerManager->warpTo(position);
    return true;
}

void set_current_monitor_blur_should_render(bool enabled) {
    if (!g_pHyprOpenGL || !g_pHyprOpenGL->m_renderData.pCurrentMonData)
        return;

    g_pHyprOpenGL->m_renderData.pCurrentMonData->blurFBShouldRender = enabled;
}

void add_rect_pass(const CRectPassElement::SRectData& data) {
    if (!g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(data));
}

void add_border_pass(const CBorderPassElement::SBorderData& data) {
    if (!g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(data));
}

void add_renderer_hints_pass(const SRenderModifData& data) {
    if (!g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->m_renderPass.add(
        makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData {data})
    );
}

void damage_window(PHLWINDOW window) {
    if (window == nullptr || !g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->damageWindow(window);
}

void reset_overview_render_guard() {
    g_overview_render_scope_depth = 0;
    g_overview_render_pass_depth = 0;
}

void begin_overview_render_pass() {
    if (!g_pHyprRenderer.get())
        return;

    if (g_overview_render_scope_depth <= 0) {
        disable_runtime_for_render_failure(
            "begin_overview_render_pass",
            "overview render pass started outside render scope"
        );
        return;
    }

    if (g_overview_render_pass_depth > 0) {
        disable_runtime_for_render_failure(
            "begin_overview_render_pass",
            "nested overview render pass detected"
        );
        return;
    }

    g_overview_render_pass_depth++;
    g_pHyprRenderer->m_renderPass.add(makeUnique<HTPassElement>());
}

void remove_clear_passes() {
    if (!g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->m_renderPass.removeAllOfType(CLEAR_PASS_ELEMENT_NAME);
}

void finalize_overview_render_pass() {
    if (g_overview_render_pass_depth <= 0) {
        if (g_overview_render_scope_depth > 0) {
            disable_runtime_for_render_failure(
                "finalize_overview_render_pass",
                "overview render pass finalized without a matching begin"
            );
        }
        g_overview_render_pass_depth = 0;
        return;
    }

    g_overview_render_pass_depth--;
}

}
