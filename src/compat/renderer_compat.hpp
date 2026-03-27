#pragma once

#include <cstdint>

#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>

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
Vector2D monitor_position(PHLMONITOR monitor);
MONITORID monitor_id(PHLMONITOR monitor);
float monitor_scale(PHLMONITOR monitor);
Vector2D monitor_transformed_size(PHLMONITOR monitor);
Vector2D monitor_pixel_size(PHLMONITOR monitor);
int monitor_transform(PHLMONITOR monitor);
PHLWORKSPACE active_monitor_workspace(PHLMONITOR monitor);
PHLMONITOR workspace_monitor(PHLWORKSPACE workspace);
bool workspace_render_visible(PHLWORKSPACE workspace);
bool workspace_is_special(PHLWORKSPACE workspace);
WORKSPACEID workspace_id(PHLWORKSPACE workspace);
PHLWORKSPACE window_workspace(PHLWINDOW window);
PHLMONITOR window_monitor(PHLWINDOW window);
Vector2D window_real_position(PHLWINDOW window);
Vector2D window_real_position_goal(PHLWINDOW window);
Vector2D window_real_size(PHLWINDOW window);
void set_window_real_position(PHLWINDOW window, const Vector2D& position);
void set_window_real_position_goal(PHLWINDOW window, const Vector2D& position);
void reset_window_workspace_move_alpha(PHLWINDOW window);
void warp_workspace_render_offset(PHLWORKSPACE workspace);
bool activate_monitor_workspace(PHLMONITOR monitor, PHLWORKSPACE workspace);
bool restore_monitor_workspace(PHLMONITOR monitor, PHLWORKSPACE workspace, bool use_change_workspace);
void set_workspace_render_visibility(PHLWORKSPACE workspace, bool visible);
PHLWORKSPACE resolve_workspace_target(PHLMONITOR monitor, WORKSPACEID workspace_id, bool create_if_missing);
bool warp_pointer(const Vector2D& position);
void begin_overview_render_pass();
void remove_clear_passes();
void finalize_overview_render_pass();

} // namespace HTCompat
