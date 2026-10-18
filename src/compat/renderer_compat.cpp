#include "renderer_compat.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <string_view>

#include "../build_contract.hpp"
#include "../globals.hpp"
#include "../logic/logic.hpp"
#include "../overview.hpp"
#include "../pass/pass_element.hpp"
#include "../plugin/guards.hpp"
#include "../runtime_fail.hpp"
#include "../scope_guard.hpp"
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

constexpr const char* CLEAR_PASS_ELEMENT_NAME = "CClearPassElement";
// Balanced by RAII in the render hooks; Hyprland renders on one thread.
int g_overview_render_scope_depth = 0;
int g_overview_render_pass_depth = 0;

bool begin_overview_render_scope(PHLMONITOR monitor, PHLWORKSPACE workspace) {
    if (g_overview_render_scope_depth > 0) {
        HTRuntimeFail::disable(
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

const HTGradientValueData& fallback_border_color(bool active) {
    static const HTGradientValueData active_fallback {CHyprColor {0x33ccffee}};
    static const HTGradientValueData inactive_fallback {CHyprColor {0x444444ee}};
    return active ? active_fallback : inactive_fallback;
}

const HTGradientValueData& border_color_from_config(bool active) {
    static auto active_color =
        CConfigValue<Config::IComplexConfigValue>("general:col.active_border");
    static auto inactive_color =
        CConfigValue<Config::IComplexConfigValue>("general:col.inactive_border");

    auto& config = active ? active_color : inactive_color;
    if (!config.good() || config.ptr() == nullptr)
        return fallback_border_color(active);

    return *static_cast<HTGradientValueData*>(config.ptr());
}

void hookRenderWorkspace(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    const auto render_original = [&] {
        HTCompat::hook_original<render_workspace_t>(g_hooks.render_workspace)(
            thisptr,
            monitor,
            workspace,
            now,
            geometry
        );
    };
    try {
        const PHTVIEW view =
            ht_manager->runtime_enabled() ? ht_manager->get_view_from_monitor(monitor) : nullptr;
        if (view == nullptr || !(view->navigating || ht_manager->has_active_view())
            || !begin_overview_render_scope(monitor, workspace)) {
            render_original();
            return;
        }

        HTScopeGuard render_scope([] { end_overview_render_scope(); });
        view->layout->render();
    } catch (...) {
        Log::logger->log(
            Log::ERR,
            "[Hyprtasking] hook_render_workspace failed: {}",
            HTPlugin::current_exception_what()
        );
        render_original();
    }
}

#if HT_HYPRLAND_GE_0_56
using render_texture_t = void (*) (
    void*,
    SP<Render::ITexture>,
    const CBox&,
    Render::GL::CHyprOpenGLImpl::STextureRenderData
);

// The renderTexture/renderBorder/blur hooks run this as their only plugin code,
// so it must not throw into the renderer.
bool overview_render_modif_active() {
    return HTPlugin::guardedValue("overview_render_modif_active", false, [] {
        if (g_pHyprRenderer == nullptr || !ht_manager->runtime_enabled())
            return false;

        const auto& render_modif = g_pHyprRenderer->m_renderData.renderModif;
        if (!render_modif.enabled || render_modif.modifs.empty())
            return false;

        if (ht_manager->has_active_view())
            return true;

        const PHLMONITOR monitor = g_pHyprRenderer->m_renderData.pMonitor.lock();
        const PHTVIEW view =
            monitor == nullptr ? nullptr : ht_manager->get_view_from_monitor(monitor);
        return view != nullptr && view->navigating;
    });
}

void hookRenderTexture(
    void* thisptr,
    SP<Render::ITexture> texture,
    const CBox& box,
    Render::GL::CHyprOpenGLImpl::STextureRenderData data
) {
    const auto original = HTCompat::hook_original<render_texture_t>(g_hooks.render_texture);

    // Every path calls this exactly once, so hand over the texture and data.
    const auto call_original = [&](const CBox& render_box) {
        original(thisptr, std::move(texture), render_box, std::move(data));
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
    HTScopeGuard restore_clip_box {[previous_clip_box] {
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
    HTScopeGuard restore_modif {[previous_modif_enabled] {
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
    HTScopeGuard restore_modif {[previous_modif_enabled] {
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
    const auto original = HTCompat::hook_original<render_border_t>(g_hooks.render_border);

    render_border_for_overview(box, data, [&](const CBox& render_box, const auto& render_data) {
        original(thisptr, render_box, gradient, render_data);
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
    const auto original = HTCompat::hook_original<render_border_lerp_t>(g_hooks.render_border_lerp);

    render_border_for_overview(box, data, [&](const CBox& render_box, const auto& render_data) {
        original(thisptr, render_box, first_gradient, second_gradient, lerp, render_data);
    });
}

bool hookBlurOptimizations(void* thisptr, PHLLS layer, PHLWINDOW window) {
    if (overview_render_modif_active())
        return false;
    return HTCompat::hook_original<blur_optimizations_t>(g_hooks.blur_optimizations)(
        thisptr,
        layer,
        window
    );
}
#endif

bool hookShouldRenderWindow(void* thisptr, PHLWINDOW window, PHLMONITOR monitor) {
    const bool original_result =
        HTCompat::hook_original<should_render_window_t>(g_hooks.should_render_window)(
            thisptr,
            window,
            monitor
        );
    return HTPlugin::guardedValue("hook_should_render_window", original_result, [&] {
        if (!ht_manager->has_active_view())
            return original_result;

        const PHTVIEW view = ht_manager->get_view_from_monitor(monitor);
        if (view == nullptr)
            return original_result;

        return view->layout->should_render_window(window, original_result);
    });
}

uint32_t hookIsSolitaryBlocked(void* thisptr, bool full) {
    // Fullscreen/solitary workspaces are otherwise skipped by Hyprland before
    // the overview can render their workspace cells.
    const uint32_t original_result =
        HTCompat::hook_original<origIsSolitaryBlocked>(g_hooks.is_solitary_blocked)(thisptr, full);
    return HTPlugin::guardedValue("hook_is_solitary_blocked", original_result, [&] {
        if (!ht_manager->runtime_enabled())
            return original_result;

        // thisptr is the monitor being asked about, not the cursor's monitor.
        const auto view =
            std::ranges::find_if(ht_manager->views, [thisptr](const PHTVIEW& candidate) {
                return candidate->get_monitor().get() == thisptr;
            });
        if (view == ht_manager->views.end())
            return original_result;

        if ((*view)->active || (*view)->navigating)
            return static_cast<uint32_t>(HTCompat::MonitorClass::SC_UNKNOWN);

        return original_result;
    });
}

} // namespace

namespace HTCompat {

bool initializeRendererHooks() {
    // Any failure rolls back every hook installed so far.
    const auto install = [](CFunctionHook*& hook, const HookSpec& spec, void* destination) {
        const auto result = install_function_hook(hook, spec, destination);
        if (result.installed)
            return true;
        HTRuntimeFail::disable("initialize_renderer_hooks", result.error);
        shutdownRendererHooks();
        return false;
    };

    if (!install(
            g_hooks.render_workspace,
            render_workspace_spec(),
            hook_destination(hookRenderWorkspace)
        ))
        return false;
#if HT_HYPRLAND_GE_0_56
    if (!install(g_hooks.render_texture, render_texture_spec(), hook_destination(hookRenderTexture))
        || !install(g_hooks.render_border, render_border_spec(), hook_destination(hookRenderBorder))
        || !install(
            g_hooks.render_border_lerp,
            render_border_lerp_spec(),
            hook_destination(hookRenderBorderLerp)
        )
        || !install(
            g_hooks.blur_optimizations,
            blur_optimizations_spec(),
            hook_destination(hookBlurOptimizations)
        ))
        return false;
#endif
    if (!install(
            g_hooks.should_render_window,
            should_render_window_spec(),
            hook_destination(hookShouldRenderWindow)
        ))
        return false;

    const auto render_window_symbol = resolve_hook_address(render_window_spec());
    if (render_window_symbol.address == nullptr) {
        HTRuntimeFail::disable(
            "initialize_renderer_hooks",
            std::format("No {}: {}", render_window_spec().label, render_window_symbol.error)
        );
        shutdownRendererHooks();
        return false;
    }
    g_hooks.render_window = render_window_symbol.address;
    Log::logger->log(LOG, "[Hyprtasking] Resolved {}", render_window_spec().signature);

    return install(
        g_hooks.is_solitary_blocked,
        solitary_blocked_spec(),
        hook_destination(hookIsSolitaryBlocked)
    );
}

void shutdownRendererHooks() {
    remove_overview_render_passes();
    remove_function_hook(g_hooks.render_workspace, "renderWorkspace");
    remove_function_hook(g_hooks.render_texture, "renderTexture");
    remove_function_hook(g_hooks.render_border, "renderBorder");
    remove_function_hook(g_hooks.render_border_lerp, "renderBorder (lerp)");
    remove_function_hook(g_hooks.blur_optimizations, "shouldUseNewBlurOptimizations");
    remove_function_hook(g_hooks.should_render_window, "shouldRenderWindow");
    remove_function_hook(g_hooks.is_solitary_blocked, "isSolitaryBlocked");
    g_hooks.render_window = nullptr;
}

void render_workspace_original(
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    hook_original<render_workspace_t>(g_hooks.render_workspace)(
        g_pHyprRenderer.get(),
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
    HTRenderPassMode mode,
    bool ignore_position,
    bool standalone
) {
    reinterpret_cast<render_window_t>(g_hooks.render_window)(
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

bool workspace_render_at_rest(PHLWORKSPACE workspace) {
    if (workspace == nullptr || !workspace->m_alpha || !workspace->m_renderOffset)
        return false;

    return !workspace->m_alpha->isBeingAnimated() && !workspace->m_renderOffset->isBeingAnimated()
        && workspace->m_alpha->value() == 1.F && workspace->m_renderOffset->value() == Vector2D {};
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

bool window_geometry_animating(PHLWINDOW window) {
    if (window == nullptr)
        return false;

#if HT_HYPRLAND_GE_0_56
    return window->positionAnimation()->isBeingAnimated()
        || window->sizeAnimation()->isBeingAnimated();
#else
    return window->m_realPosition->isBeingAnimated() || window->m_realSize->isBeingAnimated();
#endif
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

    window->alpha(Desktop::View::WINDOW_ALPHA_MOVE_TO_WORKSPACE)->setValueAndWarp(1.0);
    window->alpha(Desktop::View::WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(1.0);
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

// The overview flips every grid workspace visible and back each frame. An
// instant IN animation only warps alpha to 1 and the render offset to 0, so
// skip restarting it (config lookups, update callback, per-window warps) when
// the workspace already rests there. Hiding only needs the flag: Hyprland
// does not render workspaces that are not visible, and its own workspace
// switches restart these animations from their start values.
void set_workspace_render_visibility(PHLWORKSPACE workspace, bool visible) {
    if (workspace == nullptr)
        return;

    if (visible && !workspace_render_at_rest(workspace))
        HTCompat::start_workspace_visibility_animation(workspace, true);
    workspace->m_visible = visible;
}

const HTGradientValueData& active_border_color() {
    return border_color_from_config(true);
}

const HTGradientValueData& inactive_border_color() {
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

void begin_overview_render_pass() {
    if (!g_pHyprRenderer.get())
        return;

    if (g_overview_render_scope_depth <= 0) {
        HTRuntimeFail::disable(
            "begin_overview_render_pass",
            "overview render pass started outside render scope"
        );
        return;
    }

    if (g_overview_render_pass_depth > 0) {
        HTRuntimeFail::disable(
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
    g_overview_render_scope_depth = 0;
    g_overview_render_pass_depth = 0;
}

void finalize_overview_render_pass() {
    if (g_overview_render_pass_depth <= 0) {
        if (g_overview_render_scope_depth > 0) {
            HTRuntimeFail::disable(
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
