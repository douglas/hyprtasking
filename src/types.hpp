#pragma once

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/types.hpp>
#include <hyprutils/math/Box.hpp>

#include "build_contract.hpp"

using HTRenderModifData = Render::SRenderModifData;
using HTRenderPassMode = Render::eRenderPassMode;
using HTGradientValueData = Config::CGradientValueData;
constexpr HTRenderPassMode HT_RENDER_PASS_MAIN = Render::RENDER_PASS_MAIN;

typedef void (*render_workspace_t)(
    void* thisptr,
    PHLMONITOR pMonitor,
    PHLWORKSPACE pWorkspace,
    const Time::steady_tp& now,
    const CBox& geometry
);

typedef bool (*should_render_window_t)(void* thisptr, PHLWINDOW pWindow, PHLMONITOR pMonitor);
typedef void (*render_window_t)(
    void* thisptr,
    PHLWINDOW pWindow,
    PHLMONITOR pMonitor,
    const Time::steady_tp& time,
    bool decorate,
    HTRenderPassMode mode,
    bool ignorePosition,
    bool standalone
);

typedef long VIEWID;
