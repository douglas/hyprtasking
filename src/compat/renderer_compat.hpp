#pragma once

#include <cstdint>

#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprutils/math/Box.hpp>

enum eRenderPassMode : uint8_t;

namespace HTCompat {

void initializeRendererHooks();
void shutdownRendererHooks();

bool should_render_window_original(void* renderer, PHLWINDOW window, PHLMONITOR monitor);
void render_workspace_original(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
);
void render_window_original(
    void* thisptr,
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    eRenderPassMode mode,
    bool ignore_position,
    bool standalone
);
bool restore_monitor_workspace(PHLMONITOR monitor, PHLWORKSPACE workspace, bool use_change_workspace);
void set_workspace_render_visibility(PHLWORKSPACE workspace, bool visible);
PHLWORKSPACE resolve_workspace_target(PHLMONITOR monitor, WORKSPACEID workspace_id, bool create_if_missing);
void begin_overview_render_pass();
void remove_clear_passes();
void finalize_overview_render_pass();

} // namespace HTCompat
