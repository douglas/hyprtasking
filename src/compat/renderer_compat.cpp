#include "renderer_compat.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <limits>
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
#if HT_HYPRLAND_GE_0_57
    #include <hyprland/src/desktop/view/window/Window.hpp>
    #include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
    #include <hyprland/src/render/Context.hpp>
    #include <hyprland/src/workspace/HLWorkspace.hpp>
#endif

// 0.57 passes a Render::CRenderContext to the render entry points and moves the
// render data and the render pass onto it. These keep the hook bodies shared
// across versions; before 0.57 they expand to the global renderer state.
//   HT_CTX_PARAM: leading hook parameter     HT_CTX_TYPE: the same in a hook typedef
//   HT_CTX_ARG: leading original-call arg    HT_RENDER_DATA(ctx): SRenderData*
#if HT_HYPRLAND_GE_0_57
    #define HT_CTX_PARAM Render::CRenderContext &ctx,
    #define HT_CTX_TYPE Render::CRenderContext&,
    #define HT_CTX_ARG ctx,
    #define HT_RENDER_DATA(ctx) (&(ctx).m_data)
#else
    #define HT_CTX_PARAM
    #define HT_CTX_TYPE
    #define HT_CTX_ARG
    #define HT_RENDER_DATA(ctx) (g_pHyprRenderer ? &g_pHyprRenderer->m_renderData : nullptr)
#endif

namespace {

constexpr const char* CLEAR_PASS_ELEMENT_NAME = "CClearPassElement";
// Balanced by RAII in the render hooks; Hyprland renders on one thread.
int g_overview_render_scope_depth = 0;
int g_overview_render_pass_depth = 0;
#if HT_HYPRLAND_GE_0_57
// The context of the overview render in progress. Set for the duration of the
// renderWorkspace hook so the window and pass helpers below can reach it
// without every caller threading it through.
Render::CRenderContext* g_overview_render_ctx = nullptr;
#endif

bool begin_overview_render_scope(HT_CTX_PARAM PHLMONITOR monitor, PHLWORKSPACE workspace) {
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
#if HT_HYPRLAND_GE_0_57
    g_overview_render_ctx = &ctx;
#endif
    return true;
}

void end_overview_render_scope() {
    if (g_overview_render_scope_depth <= 0) {
        HTCompat::log(
            Log::WARN,
            "[Hyprtasking] finalize_overview_render_scope called with no active scope"
        );
        g_overview_render_scope_depth = 0;
        g_overview_render_pass_depth = 0;
#if HT_HYPRLAND_GE_0_57
        g_overview_render_ctx = nullptr;
#endif
        return;
    }

    g_overview_render_scope_depth--;
    if (g_overview_render_scope_depth == 0) {
        g_overview_render_pass_depth = 0;
#if HT_HYPRLAND_GE_0_57
        g_overview_render_ctx = nullptr;
#endif
    }
}

void add_pass_element(UP<IPassElement>&& element) {
#if HT_HYPRLAND_GE_0_57
    if (g_overview_render_ctx == nullptr)
        return;

    Render::IHyprRenderer::addPassElement(*g_overview_render_ctx, std::move(element));
#else
    g_pHyprRenderer->m_renderPass.add(std::move(element));
#endif
}

void remove_passes_of_type(const char* type) {
#if HT_HYPRLAND_GE_0_57
    if (g_overview_render_ctx == nullptr)
        return;

    Render::IHyprRenderer::currentPass(*g_overview_render_ctx).removeAllOfType(type);
#else
    g_pHyprRenderer->m_renderPass.removeAllOfType(type);
#endif
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
    HT_CTX_PARAM PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
) {
    const auto render_original = [&] {
        HTCompat::hook_original<render_workspace_t>(g_hooks.render_workspace)(
            thisptr,
            HT_CTX_ARG monitor,
            workspace,
            now,
            geometry
        );
    };
    try {
        const PHTVIEW view =
            ht_manager->runtime_enabled() ? ht_manager->get_view_from_monitor(monitor) : nullptr;
        if (view == nullptr || !(view->navigating || ht_manager->has_active_view())
            || !begin_overview_render_scope(HT_CTX_ARG monitor, workspace)) {
            render_original();
            return;
        }

        HTScopeGuard render_scope([] { end_overview_render_scope(); });
        view->layout->render();
    } catch (...) {
        HTCompat::log(
            Log::ERR,
            "[Hyprtasking] hook_render_workspace failed: {}",
            HTPlugin::current_exception_what()
        );
        render_original();
    }
}

#if HT_HYPRLAND_GE_0_56
using render_texture_t = void (*)(
    void*,
    HT_CTX_TYPE SP<Render::ITexture>,
    const CBox&,
    Render::GL::CHyprOpenGLImpl::STextureRenderData
);

// The renderTexture/renderBorder/blur hooks run this as their only plugin code,
// so it must not throw into the renderer.
bool overview_render_modif_active(const Render::SRenderData* render_data) {
    return HTPlugin::guardedValue("overview_render_modif_active", false, [render_data] {
        if (render_data == nullptr || !ht_manager->runtime_enabled())
            return false;

        const auto& render_modif = render_data->renderModif;
        if (!render_modif.enabled || render_modif.modifs.empty())
            return false;

        if (ht_manager->has_active_view())
            return true;

        const PHLMONITOR monitor = render_data->pMonitor.lock();
        const PHTVIEW view =
            monitor == nullptr ? nullptr : ht_manager->get_view_from_monitor(monitor);
        return view != nullptr && view->navigating;
    });
}

void hookRenderTexture(
    void* thisptr,
    HT_CTX_PARAM SP<Render::ITexture> texture,
    const CBox& box,
    Render::GL::CHyprOpenGLImpl::STextureRenderData data
) {
    const auto original = HTCompat::hook_original<render_texture_t>(g_hooks.render_texture);

    // Every path calls this exactly once, so hand over the texture and data.
    const auto call_original = [&](const CBox& render_box) {
        original(thisptr, HT_CTX_ARG std::move(texture), render_box, std::move(data));
    };
    const auto render_data = HT_RENDER_DATA(ctx);
    if (!overview_render_modif_active(render_data)) {
        call_original(box);
        return;
    }

    const PHLMONITOR monitor = render_data->pMonitor.lock();
    if (monitor == nullptr) {
        call_original(box);
        return;
    }

    CRegion full_damage {0, 0, monitor->m_transformedSize.x, monitor->m_transformedSize.y};
    data.damage = &full_damage;
    data.clipRegion = {};
    const CBox previous_clip_box = render_data->clipBox;
    render_data->clipBox = {};
    HTScopeGuard restore_clip_box {[render_data, previous_clip_box] {
        render_data->clipBox = previous_clip_box;
    }};

    auto& render_modif = render_data->renderModif;
    if (!data.blur) {
        call_original(box);
        return;
    }

    CBox transformed_box = box;
    render_modif.applyToBox(transformed_box);
    const bool previous_modif_enabled = render_modif.enabled;
    render_modif.enabled = false;
    HTScopeGuard restore_modif {[render_data, previous_modif_enabled] {
        render_data->renderModif.enabled = previous_modif_enabled;
    }};
    call_original(transformed_box);
}

using render_border_t = void (*)(
    void*,
    HT_CTX_TYPE const CBox&,
    const Config::CGradientValueData&,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData
);
using render_border_lerp_t = void (*)(
    void*,
    HT_CTX_TYPE const CBox&,
    const Config::CGradientValueData&,
    const Config::CGradientValueData&,
    float,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData
);
using blur_optimizations_t = bool (*)(void*, HT_CTX_TYPE PHLLS, PHLWINDOW);

template<typename CallOriginal>
void render_border_for_overview(
    Render::SRenderData* render_data,
    const CBox& box,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData& data,
    CallOriginal&& call_original
) {
    if (!overview_render_modif_active(render_data)) {
        call_original(box, data);
        return;
    }

    auto& render_modif = render_data->renderModif;
    CBox transformed_box = box;
    render_modif.applyToBox(transformed_box);
    data.borderSize = std::round(data.borderSize * render_modif.combinedScale());
    const bool previous_modif_enabled = render_modif.enabled;
    render_modif.enabled = false;
    HTScopeGuard restore_modif {[render_data, previous_modif_enabled] {
        render_data->renderModif.enabled = previous_modif_enabled;
    }};
    call_original(transformed_box, data);
}

void hookRenderBorder(
    void* thisptr,
    HT_CTX_PARAM const CBox& box,
    const Config::CGradientValueData& gradient,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData data
) {
    const auto original = HTCompat::hook_original<render_border_t>(g_hooks.render_border);

    render_border_for_overview(
        HT_RENDER_DATA(ctx),
        box,
        data,
        [&](const CBox& render_box, const auto& border_data) {
            original(thisptr, HT_CTX_ARG render_box, gradient, border_data);
        }
    );
}

void hookRenderBorderLerp(
    void* thisptr,
    HT_CTX_PARAM const CBox& box,
    const Config::CGradientValueData& first_gradient,
    const Config::CGradientValueData& second_gradient,
    float lerp,
    Render::GL::CHyprOpenGLImpl::SBorderRenderData data
) {
    const auto original = HTCompat::hook_original<render_border_lerp_t>(g_hooks.render_border_lerp);

    render_border_for_overview(
        HT_RENDER_DATA(ctx),
        box,
        data,
        [&](const CBox& render_box, const auto& border_data) {
            original(
                thisptr,
                HT_CTX_ARG render_box,
                first_gradient,
                second_gradient,
                lerp,
                border_data
            );
        }
    );
}

bool hookBlurOptimizations(void* thisptr, HT_CTX_PARAM PHLLS layer, PHLWINDOW window) {
    if (overview_render_modif_active(HT_RENDER_DATA(ctx)))
        return false;
    return HTCompat::hook_original<blur_optimizations_t>(g_hooks.blur_optimizations)(
        thisptr,
        HT_CTX_ARG layer,
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
    HTCompat::log(LOG, "[Hyprtasking] Resolved {}", render_window_spec().signature);

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
#if HT_HYPRLAND_GE_0_57
    if (g_overview_render_ctx == nullptr) {
        HTRuntimeFail::disable(
            "render_workspace_original",
            "workspace render requested outside an overview render"
        );
        return;
    }
#endif
    hook_original<render_workspace_t>(g_hooks.render_workspace)(
        g_pHyprRenderer.get(),
#if HT_HYPRLAND_GE_0_57
        *g_overview_render_ctx,
#endif
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
#if HT_HYPRLAND_GE_0_57
    if (window == nullptr || g_overview_render_ctx == nullptr) {
        HTRuntimeFail::disable(
            "render_window_original",
            "window render requested without a window or outside an overview render"
        );
        return;
    }
#endif
    reinterpret_cast<render_window_t>(g_hooks.render_window)(
        g_pHyprRenderer.get(),
#if HT_HYPRLAND_GE_0_57
        *g_overview_render_ctx,
#endif
        window,
        monitor,
#if HT_HYPRLAND_GE_0_57
        window->presentation().renderPresentation(),
#endif
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

#if HT_HYPRLAND_GE_0_57
    return workspace->visible();
#else
    return workspace->m_visible;
#endif
}

#if HT_HYPRLAND_GE_0_57
namespace {

    // Numbered workspace IDs are positive and fit the 32-bit container.
    bool is_numbered_workspace_id(WORKSPACEID workspace_id) {
        return workspace_id > 0
            && workspace_id <= std::numeric_limits<::Workspace::WorkspaceIDContainer>::max();
    }

    ::Workspace::SWorkspaceNumberedID to_numbered_workspace_id(WORKSPACEID workspace_id) {
        return ::Workspace::SWorkspaceNumberedID(
            static_cast<::Workspace::WorkspaceIDContainer>(workspace_id)
        );
    }

} // namespace
#endif

namespace {

    // 0.57 workspaces announce themselves once fully set up (ready() is safe to
    // repeat), so call this after a workspace was created and then activated or
    // given a window. Before 0.57 there is nothing to announce.
    void mark_workspace_ready([[maybe_unused]] const PHLWORKSPACE& workspace) {
#if HT_HYPRLAND_GE_0_57
        if (workspace != nullptr)
            workspace->ready();
#endif
    }

} // namespace

WORKSPACEID workspace_id(PHLWORKSPACE workspace) {
    if (workspace == nullptr)
        return WORKSPACE_INVALID;

#if HT_HYPRLAND_GE_0_57
    // Named and special workspaces have no numbered ID.
    const auto numbered_id = workspace->numberedID();
    return numbered_id.has_value() ? static_cast<WORKSPACEID>(*numbered_id) : WORKSPACE_INVALID;
#else
    return workspace->m_id;
#endif
}

PHLWORKSPACE workspace_by_id(WORKSPACEID workspace_id) {
#if HT_HYPRLAND_GE_0_57
    if (!is_numbered_workspace_id(workspace_id) || !State::workspaceState())
        return nullptr;

    return State::workspaceState()->query().numbered(to_numbered_workspace_id(workspace_id)).run();
#elif HT_HYPRLAND_GE_0_56
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

#if HT_HYPRLAND_GE_0_57
    return workspace_id(window->m_workspace);
#else
    return window->workspaceID();
#endif
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

#if HT_HYPRLAND_GE_0_57
    auto& presentation = window->presentation();
    presentation.alpha(Desktop::View::WINDOW_ALPHA_MOVE_TO_WORKSPACE)->setValueAndWarp(1.0);
    presentation.alpha(Desktop::View::WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(1.0);
#else
    window->alpha(Desktop::View::WINDOW_ALPHA_MOVE_TO_WORKSPACE)->setValueAndWarp(1.0);
    window->alpha(Desktop::View::WINDOW_ALPHA_MOVE_FROM_WORKSPACE)->setValueAndWarp(1.0);
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
    mark_workspace_ready(workspace);
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

    mark_workspace_ready(workspace);
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
#if HT_HYPRLAND_GE_0_57
    workspace->setVisible(visible);
#else
    workspace->m_visible = visible;
#endif
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
#if HT_HYPRLAND_GE_0_57
        if (!State::workspaceState() || !is_numbered_workspace_id(workspace_id))
            return nullptr;

        workspace = State::workspaceState()->createNumbered(
            to_numbered_workspace_id(workspace_id),
            monitor
        );
#elif HT_HYPRLAND_GE_0_56
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

    add_pass_element(makeUnique<CRectPassElement>(data));
}

void add_border_pass(const CBorderPassElement::SBorderData& data) {
    if (!g_pHyprRenderer.get())
        return;

    add_pass_element(makeUnique<CBorderPassElement>(data));
}

void add_renderer_hints_pass(const HTRenderModifData& data) {
    if (!g_pHyprRenderer.get())
        return;

    add_pass_element(
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
    add_pass_element(makeUnique<HTPassElement>());
}

void remove_clear_passes() {
    if (!g_pHyprRenderer.get())
        return;

    remove_passes_of_type(CLEAR_PASS_ELEMENT_NAME);
}

void remove_overview_render_passes() {
    if (!g_pHyprRenderer.get())
        return;

    // 0.57 keeps the pass on the render context, which resets every frame, so
    // there is nothing left to remove once the overview render scope ended.
    remove_passes_of_type(HT_PASS_ELEMENT_NAME);
    remove_passes_of_type(CLEAR_PASS_ELEMENT_NAME);
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
