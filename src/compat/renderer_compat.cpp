#include "renderer_compat.hpp"

#include <cmath>
#include <format>
#include <hyprland/src/config/ConfigValue.hpp>
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
#include "animation_compat.hpp"
#include "monitor_compat.hpp"
#include "pointer_compat.hpp"
#include "profile.hpp"

#if HT_HYPRLAND_GE_0_56
    #include <hyprland/src/state/WorkspacePlacementController.hpp>
    #include <hyprland/src/state/WorkspaceState.hpp>
#endif

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
        return static_cast<uint32_t>(HTCompat::MonitorClass::SC_UNKNOWN);
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

#if HT_HYPRLAND_GE_0_56
using render_texture_t = void (*) (
    void*,
    SP<Render::ITexture>,
    const CBox&,
    Render::GL::CHyprOpenGLImpl::STextureRenderData
);

bool overview_render_modif_active() {
    if (g_pHyprRenderer == nullptr || ht_manager == nullptr || !ht_manager->runtime_enabled())
        return false;

    const auto& render_modif = g_pHyprRenderer->m_renderData.renderModif;
    if (!render_modif.enabled || render_modif.modifs.empty())
        return false;

    if (ht_manager->has_active_view())
        return true;

    const PHLMONITOR monitor = g_pHyprRenderer->m_renderData.pMonitor.lock();
    const PHTVIEW view = monitor == nullptr ? nullptr : ht_manager->get_view_from_monitor(monitor);
    return view != nullptr && view->navigating;
}

void hookRenderTexture(
    void* thisptr,
    SP<Render::ITexture> texture,
    const CBox& box,
    Render::GL::CHyprOpenGLImpl::STextureRenderData data
) {
    if (render_texture_hook == nullptr || render_texture_hook->m_original == nullptr) {
        disable_runtime_for_render_failure("hook_render_texture", "missing renderTexture original call-through");
        return;
    }

    const auto call_original = [&](const CBox& render_box) {
        ((render_texture_t)render_texture_hook->m_original)(thisptr, texture, render_box, data);
    };
    if (!overview_render_modif_active()) {
        call_original(box);
        return;
    }

    const PHLMONITOR monitor = g_pHyprRenderer->m_renderData.pMonitor.lock();
    if (monitor == nullptr) {
        call_original(box);
        return;
    }

    CRegion full_damage {0, 0, monitor->m_transformedSize.x, monitor->m_transformedSize.y};
    data.damage = &full_damage;
    data.clipRegion = {};
    const CBox previous_clip_box = g_pHyprRenderer->m_renderData.clipBox;
    g_pHyprRenderer->m_renderData.clipBox = {};
    CScopeGuard restore_clip_box {[previous_clip_box] {
        g_pHyprRenderer->m_renderData.clipBox = previous_clip_box;
    }};

    auto& render_modif = g_pHyprRenderer->m_renderData.renderModif;
    if (!data.blur) {
        call_original(box);
        return;
    }

    CBox transformed_box = box;
    render_modif.applyToBox(transformed_box);
    const bool previous_modif_enabled = render_modif.enabled;
    render_modif.enabled = false;
    CScopeGuard restore_modif {[previous_modif_enabled] {
        g_pHyprRenderer->m_renderData.renderModif.enabled = previous_modif_enabled;
    }};
    call_original(transformed_box);
}

using render_border_t = void (*) (
    void*,
    const CBox&,
    const Config::CGradientValueData&,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData
);
using render_border_lerp_t = void (*) (
    void*,
    const CBox&,
    const Config::CGradientValueData&,
    const Config::CGradientValueData&,
    float,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData
);
using blur_optimizations_t = bool (*) (void*, PHLLS, PHLWINDOW);

template <typename CallOriginal>
void render_border_for_overview(
    const CBox& box,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData& data,
    CallOriginal&& call_original
) {
    if (!overview_render_modif_active()) {
        call_original(box, data);
        return;
    }

    auto& render_modif = g_pHyprRenderer->m_renderData.renderModif;
    CBox transformed_box = box;
    render_modif.applyToBox(transformed_box);
    data.borderSize = std::round(data.borderSize * render_modif.combinedScale());
    const bool previous_modif_enabled = render_modif.enabled;
    render_modif.enabled = false;
    CScopeGuard restore_modif {[previous_modif_enabled] {
        g_pHyprRenderer->m_renderData.renderModif.enabled = previous_modif_enabled;
    }};
    call_original(transformed_box, data);
}

void hookRenderBorder(
    void* thisptr,
    const CBox& box,
    const Config::CGradientValueData& gradient,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData data
) {
    render_border_for_overview(box, data, [&](const CBox& render_box, const auto& render_data) {
        ((render_border_t)render_border_hook->m_original)(thisptr, render_box, gradient, render_data);
    });
}

void hookRenderBorderLerp(
    void* thisptr,
    const CBox& box,
    const Config::CGradientValueData& first_gradient,
    const Config::CGradientValueData& second_gradient,
    float lerp,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData data
) {
    render_border_for_overview(box, data, [&](const CBox& render_box, const auto& render_data) {
        ((render_border_lerp_t)render_border_lerp_hook->m_original)(
            thisptr,
            render_box,
            first_gradient,
            second_gradient,
            lerp,
            render_data
        );
    });
}

bool hookBlurOptimizations(void* thisptr, PHLLS layer, PHLWINDOW window) {
    if (overview_render_modif_active())
        return false;
    return ((blur_optimizations_t)blur_optimizations_hook->m_original)(thisptr, layer, window);
}
#endif

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
                return static_cast<uint32_t>(HTCompat::MonitorClass::SC_UNKNOWN);

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

#if HT_HYPRLAND_GE_0_56
    const auto render_texture = install_function_hook(
        render_texture_hook,
        render_texture_spec(),
        (void*)hookRenderTexture
    );
    if (!render_texture.installed) {
        disable_runtime_for_render_failure("initialize_renderer_hooks", render_texture.error);
        shutdownRendererHooks();
        return false;
    }
    render_texture_hook_info = {
        .signature = render_texture.signature,
        .method = render_texture.method,
    };

    const auto render_border = install_function_hook(
        render_border_hook,
        render_border_spec(),
        (void*)hookRenderBorder
    );
    if (!render_border.installed) {
        disable_runtime_for_render_failure("initialize_renderer_hooks", render_border.error);
        shutdownRendererHooks();
        return false;
    }
    render_border_hook_info = {
        .signature = render_border.signature,
        .method = render_border.method,
    };

    const auto render_border_lerp = install_function_hook(
        render_border_lerp_hook,
        render_border_lerp_spec(),
        (void*)hookRenderBorderLerp
    );
    if (!render_border_lerp.installed) {
        disable_runtime_for_render_failure("initialize_renderer_hooks", render_border_lerp.error);
        shutdownRendererHooks();
        return false;
    }
    render_border_lerp_hook_info = {
        .signature = render_border_lerp.signature,
        .method = render_border_lerp.method,
    };

    const auto blur_optimizations = install_function_hook(
        blur_optimizations_hook,
        blur_optimizations_spec(),
        (void*)hookBlurOptimizations
    );
    if (!blur_optimizations.installed) {
        disable_runtime_for_render_failure("initialize_renderer_hooks", blur_optimizations.error);
        shutdownRendererHooks();
        return false;
    }
    blur_optimizations_hook_info = {
        .signature = blur_optimizations.signature,
        .method = blur_optimizations.method,
    };
#endif

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
    remove_function_hook(render_texture_hook, "renderTexture");
    remove_function_hook(render_border_hook, "renderBorder");
    remove_function_hook(render_border_lerp_hook, "renderBorder (lerp)");
    remove_function_hook(blur_optimizations_hook, "shouldUseNewBlurOptimizations");
    remove_function_hook(should_render_window_hook, "shouldRenderWindow");
    remove_function_hook(is_solitary_blocked_hook, "isSolitaryBlocked");
    render_window = nullptr;
    render_workspace_hook_info = {};
    render_texture_hook_info = {};
    render_border_hook_info = {};
    render_border_lerp_hook_info = {};
    blur_optimizations_hook_info = {};
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

PHLWORKSPACE workspace_by_id(WORKSPACEID workspace_id) {
#if HT_HYPRLAND_GE_0_56
    if (workspace_id == WORKSPACE_INVALID || !State::workspaceState())
        return nullptr;

    return State::workspaceState()->query().id(workspace_id).run();
#else
    if (workspace_id == WORKSPACE_INVALID || !g_pCompositor)
        return nullptr;

    return g_pCompositor->getWorkspaceByID(workspace_id);
#endif
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

#if HT_HYPRLAND_GE_0_56
    return window->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
#else
    return window->m_realPosition->value();
#endif
}

Vector2D window_real_position_goal(PHLWINDOW window) {
    if (window == nullptr)
        return {};

#if HT_HYPRLAND_GE_0_56
    return window->position(Desktop::View::IGeometric::GEOMETRIC_GOAL);
#else
    return window->m_realPosition->goal();
#endif
}

Vector2D window_real_size(PHLWINDOW window) {
    if (window == nullptr)
        return {};

#if HT_HYPRLAND_GE_0_56
    return window->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT);
#else
    return window->m_realSize->value();
#endif
}

void set_window_real_position(PHLWINDOW window, const Vector2D& position) {
    if (window == nullptr)
        return;

#if HT_HYPRLAND_GE_0_56
    window->positionAnimation()->setValueAndWarp(position);
#else
    window->m_realPosition->setValueAndWarp(position);
#endif
}

void set_window_real_position_goal(PHLWINDOW window, const Vector2D& position) {
    if (window == nullptr)
        return;

#if HT_HYPRLAND_GE_0_56
    *window->positionAnimation() = position;
#else
    *window->m_realPosition = position;
#endif
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

    HTCompat::start_workspace_visibility_animation(workspace, visible);
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
    if (workspace == nullptr && create_if_missing) {
#if HT_HYPRLAND_GE_0_56
        if (!State::workspaceState())
            return nullptr;

        workspace = State::workspaceState()->create(workspace_id, HTCompat::monitor_id(monitor));
#else
        workspace = g_pCompositor->createNewWorkspace(workspace_id, HTCompat::monitor_id(monitor));
#endif
    }

    return workspace;
}

bool move_workspace_to_monitor(PHLWORKSPACE workspace, PHLMONITOR monitor, bool no_warp_cursor) {
    if (workspace == nullptr || monitor == nullptr)
        return false;

#if HT_HYPRLAND_GE_0_56
    if (!State::workspacePlacementController())
        return false;

    State::workspacePlacementController()->moveWorkspaceToMonitor(workspace, monitor, no_warp_cursor);
#else
    if (!g_pCompositor)
        return false;

    g_pCompositor->moveWorkspaceToMonitor(workspace, monitor, no_warp_cursor);
#endif
    return true;
}

bool warp_pointer(const Vector2D& position) {
    const auto pointer_manager = HTCompat::pointer_manager();
    if (!HTLogic::isFinitePoint(position.x, position.y) || pointer_manager == nullptr)
        return false;

    pointer_manager->warpTo(position);
    return true;
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
