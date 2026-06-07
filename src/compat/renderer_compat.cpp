#include "renderer_compat.hpp"

#include <format>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <string_view>

#include "../build_contract.hpp"
#include "../globals.hpp"
#include "../logic/geometry_model.hpp"
#include "../overview.hpp"
#include "../pass/pass_element.hpp"
#include "../plugin/guards.hpp"
#include "../runtime_fail.hpp"
#include "../types.hpp"
#include "profile.hpp"

namespace {

const std::string CLEAR_PASS_ELEMENT_NAME = "CClearPassElement";
thread_local int g_overview_render_scope_depth = 0;
thread_local int g_overview_render_pass_depth = 0;

using Hyprutils::Utils::CScopeGuard;

void disable_runtime_for_render_failure(std::string_view source, std::string_view reason) {
    HTRuntimeFail::disable(source, reason);
}

void render_workspace_original_or_disable(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    if (HTCompat::render_workspace_original(thisptr, monitor, workspace, now, geometry))
        return;

    disable_runtime_for_render_failure(
        "hook_render_workspace",
        "missing renderWorkspace original call-through"
    );
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
    if (thisptr == nullptr || is_solitary_blocked_hook == nullptr
        || is_solitary_blocked_hook->m_original == nullptr) {
        disable_runtime_for_render_failure(
            "hook_is_solitary_blocked",
            "missing isSolitaryBlocked original call-through"
        );
        return static_cast<uint32_t>(CMonitor::SC_UNKNOWN);
    }

    return (*(origIsSolitaryBlocked)is_solitary_blocked_hook->m_original)(thisptr, full);
}

HTGradientValueData fallback_border_color(bool active) {
    return HTGradientValueData {CHyprColor {active ? 0x33ccffee : 0x444444ee}};
}

#if HT_HYPRLAND_GE_0_55
HTGradientValueData border_color_from_config(bool active) {
    static auto active_color =
        CConfigValue<Config::IComplexConfigValue>("general:col.active_border");
    static auto inactive_color =
        CConfigValue<Config::IComplexConfigValue>("general:col.inactive_border");

    auto& config = active ? active_color : inactive_color;
    if (!config.good() || config.ptr() == nullptr)
        return fallback_border_color(active);

    return *static_cast<HTGradientValueData*>(config.ptr());
}
#else
HTGradientValueData border_color_from_config(bool active) {
    static auto active_color = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
    static auto inactive_color = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");

    auto& config = active ? active_color : inactive_color;
    if (config.ptr() == nullptr || config.ptr()->getData() == nullptr)
        return fallback_border_color(active);

    return *static_cast<HTGradientValueData*>(config.ptr()->getData());
}
#endif

void hookRenderWorkspace(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    try {
        if (ht_manager == nullptr) {
            render_workspace_original_or_disable(thisptr, monitor, workspace, now, geometry);
            return;
        }
        if (!ht_manager->runtime_enabled()) {
            render_workspace_original_or_disable(thisptr, monitor, workspace, now, geometry);
            return;
        }

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view != nullptr && view->get_monitor() != monitor) {
            Log::logger->log(
                Log::WARN,
                "[Hyprtasking] hook_render_workspace monitor/view mismatch for view {}",
                view->monitor_id
            );
            render_workspace_original_or_disable(thisptr, monitor, workspace, now, geometry);
            return;
        }
        if (view != nullptr && (view->navigating || ht_manager->has_active_view())) {
            if (!begin_overview_render_scope(monitor, workspace)) {
                render_workspace_original_or_disable(thisptr, monitor, workspace, now, geometry);
                return;
            }

            CScopeGuard render_scope([] { end_overview_render_scope(); });
            view->layout->render();
            return;
        }

        render_workspace_original_or_disable(thisptr, monitor, workspace, now, geometry);
    } catch (const std::exception& e) {
        Log::logger->log(Log::ERR, "[Hyprtasking] hook_render_workspace failed: {}", e.what());
        render_workspace_original_or_disable(thisptr, monitor, workspace, now, geometry);
    } catch (...) {
        Log::logger->log(
            Log::ERR,
            "[Hyprtasking] hook_render_workspace failed with unknown exception"
        );
        render_workspace_original_or_disable(thisptr, monitor, workspace, now, geometry);
    }
}

bool hookShouldRenderWindow(void* thisptr, PHLWINDOW window, PHLMONITOR monitor) {
    const bool original_result = HTCompat::should_render_window_original(thisptr, window, monitor);
    return HTPlugin::guardedValue("hook_should_render_window", original_result, [&] {
        if (ht_manager == nullptr || !ht_manager->runtime_enabled()
            || !ht_manager->has_active_view())
            return original_result;

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view == nullptr || view->get_monitor() != monitor)
            return original_result;

        return view->layout->should_render_window(window);
    });
}

uint32_t hookIsSolitaryBlocked(void* thisptr, bool full) {
    // Fullscreen/solitary workspaces are otherwise skipped by Hyprland before
    // the overview can render their workspace cells.
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

} // namespace

namespace HTCompat {

bool initializeRendererHooks() {
    const auto render_workspace = install_function_hook(
        render_workspace_hook,
        render_workspace_spec(),
        (void*)hookRenderWorkspace
    );
    if (!render_workspace.installed) {
        disable_runtime_for_render_failure("initialize_renderer_hooks", render_workspace.error);
        return false;
    }
    render_workspace_hook_info = {
        .signature = render_workspace.signature,
        .method = render_workspace.method,
    };

    const auto should_render_window = install_function_hook(
        should_render_window_hook,
        should_render_window_spec(),
        (void*)hookShouldRenderWindow
    );
    if (!should_render_window.installed) {
        disable_runtime_for_render_failure("initialize_renderer_hooks", should_render_window.error);
        shutdownRendererHooks();
        return false;
    }
    should_render_window_hook_info = {
        .signature = should_render_window.signature,
        .method = should_render_window.method,
    };

    const auto render_window_symbol = resolve_hook_address(render_window_spec());
    if (render_window_symbol.address == nullptr) {
        disable_runtime_for_render_failure(
            "initialize_renderer_hooks",
            std::format("No {}: {}", render_window_spec().label, render_window_symbol.error)
        );
        shutdownRendererHooks();
        return false;
    }
    render_window = render_window_symbol.address;
    render_window_symbol_info = {
        .signature = render_window_symbol.signature,
        .method = render_window_symbol.method,
    };
    Log::logger->log(
        LOG,
        "[Hyprtasking] Resolved {} via {}",
        render_window_symbol.signature,
        render_window_symbol.method
    );

    const auto solitary_blocked_hook = install_function_hook(
        is_solitary_blocked_hook,
        solitary_blocked_spec(),
        (void*)hookIsSolitaryBlocked
    );
    if (!solitary_blocked_hook.installed) {
        disable_runtime_for_render_failure(
            "initialize_renderer_hooks",
            solitary_blocked_hook.error
        );
        shutdownRendererHooks();
        return false;
    }
    is_solitary_blocked_hook_info = {
        .signature = solitary_blocked_hook.signature,
        .method = solitary_blocked_hook.method,
    };

    return true;
}

void shutdownRendererHooks() {
    remove_overview_render_passes();
    remove_function_hook(render_workspace_hook, "renderWorkspace");
    remove_function_hook(should_render_window_hook, "shouldRenderWindow");
    remove_function_hook(is_solitary_blocked_hook, "isSolitaryBlocked");
    render_window = nullptr;
    render_workspace_hook_info = {};
    should_render_window_hook_info = {};
    render_window_symbol_info = {};
    is_solitary_blocked_hook_info = {};
}

bool should_render_window_original(PHLWINDOW window, PHLMONITOR monitor) {
    return should_render_window_original(g_pHyprRenderer.get(), window, monitor);
}

bool should_render_window_original(void* renderer, PHLWINDOW window, PHLMONITOR monitor) {
    if (renderer == nullptr || should_render_window_hook == nullptr || window == nullptr
        || monitor == nullptr)
        return false;
    if (should_render_window_hook->m_original == nullptr) {
        disable_runtime_for_render_failure(
            "should_render_window_original",
            "missing shouldRenderWindow original call-through"
        );
        return true;
    }

    return ((should_render_window_t)(should_render_window_hook->m_original))(
        renderer,
        window,
        monitor
    );
}

bool render_workspace_original(
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    return render_workspace_original(g_pHyprRenderer.get(), monitor, workspace, now, geometry);
}

bool render_workspace_original(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    if (thisptr == nullptr || render_workspace_hook == nullptr)
        return false;
    if (render_workspace_hook->m_original == nullptr)
        return false;

    ((render_workspace_t)(render_workspace_hook
                              ->m_original))(thisptr, monitor, workspace, now, geometry);
    return true;
}

bool render_window_original(
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    HTRenderPassMode mode,
    bool ignore_position,
    bool standalone
) {
    return render_window_original(
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

bool render_window_original(
    void* thisptr,
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    HTRenderPassMode mode,
    bool ignore_position,
    bool standalone
) {
    if (thisptr == nullptr || render_window == nullptr || window == nullptr || monitor == nullptr)
        return false;

    ((
        render_window_t
    )render_window)(thisptr, window, monitor, time, decorate, mode, ignore_position, standalone);
    return true;
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

CBox monitor_logical_box(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return {};

    return monitor->logicalBox();
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

MONITORID workspace_monitor_id(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return MONITOR_INVALID;

    return workspace->monitorID();
}

bool workspace_render_visible(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return false;

    return workspace->m_visible;
}

WORKSPACEID workspace_id(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return WORKSPACE_INVALID;

    return workspace->m_id;
}

int workspace_window_count(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return 0;

    return workspace->getWindows();
}

std::vector<PHLWORKSPACE> workspaces_for_monitor(PHLMONITOR monitor) {
    std::vector<PHLWORKSPACE> result;
    if (monitor == nullptr || !g_pCompositor)
        return result;

    const MONITORID mid = monitor_id(monitor);
    for (const auto& ws : g_pCompositor->getWorkspacesCopy()) {
        if (ws != nullptr && !ws->m_isSpecialWorkspace && workspace_monitor_id(ws) == mid)
            result.push_back(ws);
    }
    return result;
}

PHLWORKSPACE workspace_by_id(WORKSPACEID workspace_id) {
    if (workspace_id == WORKSPACE_INVALID || !g_pCompositor)
        return nullptr;

    return g_pCompositor->getWorkspaceByID(workspace_id);
}

PHLWORKSPACE window_workspace(PHLWINDOW window) {
    if (window == nullptr)
        return nullptr;

    return window->m_workspace;
}

WORKSPACEID window_workspace_id(PHLWINDOW window) {
    if (window == nullptr)
        return WORKSPACE_INVALID;

    return window->workspaceID();
}

PHLMONITOR window_monitor(PHLWINDOW window) {
    if (window == nullptr)
        return nullptr;

    return window->m_monitor.lock();
}

CBox window_main_surface_box(PHLWINDOW window) {
    if (window == nullptr)
        return {};

    return window->getWindowMainSurfaceBox();
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

#if HT_HYPRLAND_GE_0_55
    window->alpha(Desktop::View::WINDOW_ALPHA_MOVE_TO_WORKSPACE)->setValueAndWarp(1.0);
    window->alpha(Desktop::View::WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(1.0);
#else
    window->m_movingToWorkspaceAlpha->setValueAndWarp(1.0);
    window->m_movingFromWorkspaceAlpha->setValueAndWarp(1.0);
#endif
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

bool restore_monitor_workspace(
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    bool use_change_workspace
) {
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

    if (g_pDesktopAnimationManager) {
        g_pDesktopAnimationManager->startAnimation(
            workspace,
            visible ? CDesktopAnimationManager::ANIMATION_TYPE_IN
                    : CDesktopAnimationManager::ANIMATION_TYPE_OUT,
            false,
            true
        );
    }
    workspace->m_visible = visible;
}

HTGradientValueData active_border_color() {
    return border_color_from_config(true);
}

HTGradientValueData inactive_border_color() {
    return border_color_from_config(false);
}

PHLWORKSPACE
resolve_workspace_target(PHLMONITOR monitor, WORKSPACEID workspace_id, bool create_if_missing) {
    if (monitor == nullptr || workspace_id == WORKSPACE_INVALID || !g_pCompositor)
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

SP<Render::ITexture> monitor_background_texture(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return nullptr;

    return monitor->m_background;
}

void add_texture_pass(const CTexPassElement::SRenderData& data) {
    if (!g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(data));
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

void add_renderer_hints_pass(const HTRenderModifData& data) {
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

void remove_overview_render_passes() {
    if (!g_pHyprRenderer.get())
        return;

    g_pHyprRenderer->m_renderPass.removeAllOfType(HT_PASS_ELEMENT_NAME);
    g_pHyprRenderer->m_renderPass.removeAllOfType(CLEAR_PASS_ELEMENT_NAME);
    reset_overview_render_guard();
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

} // namespace HTCompat
