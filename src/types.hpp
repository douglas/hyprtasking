#pragma once

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/types.hpp>
#include <hyprutils/math/Box.hpp>

#include "build_contract.hpp"

#if HT_HYPRLAND_GE_0_57
    #include <hyprland/src/render/Context.hpp>
    #include <hyprland/src/render/WindowRenderPresentation.hpp>
#endif

using HTRenderModifData = Render::SRenderModifData;
using HTRenderPassMode = Render::eRenderPassMode;
using HTGradientValueData = Config::CGradientValueData;
constexpr HTRenderPassMode HT_RENDER_PASS_MAIN = Render::RENDER_PASS_MAIN;

using should_render_window_t = bool (*)(void* thisptr, PHLWINDOW pWindow, PHLMONITOR pMonitor);

// 0.57 threads a Render::CRenderContext through the render entry points.
#if HT_HYPRLAND_GE_0_57
using render_workspace_t = void (*)(
    void* thisptr,
    Render::CRenderContext& ctx,
    PHLMONITOR pMonitor,
    PHLWORKSPACE pWorkspace,
    const Time::steady_tp& now,
    const CBox& geometry
);

using render_window_t = void (*)(
    void* thisptr,
    Render::CRenderContext& ctx,
    PHLWINDOW pWindow,
    PHLMONITOR pMonitor,
    const Render::SWindowRenderPresentation& presentation,
    const Time::steady_tp& time,
    bool decorate,
    HTRenderPassMode mode,
    bool ignorePosition,
    bool standalone
);
#else
using render_workspace_t = void (*)(
    void* thisptr,
    PHLMONITOR pMonitor,
    PHLWORKSPACE pWorkspace,
    const Time::steady_tp& now,
    const CBox& geometry
);

using render_window_t = void (*)(
    void* thisptr,
    PHLWINDOW pWindow,
    PHLMONITOR pMonitor,
    const Time::steady_tp& time,
    bool decorate,
    HTRenderPassMode mode,
    bool ignorePosition,
    bool standalone
);
#endif

using VIEWID = long;
