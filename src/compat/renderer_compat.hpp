#pragma once

#include <cstdint>
#include <hyprland/src/SharedDefs.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprutils/math/Box.hpp>
#include <hyprutils/math/Vector2D.hpp>

#include "../types.hpp"

namespace HTCompat {

bool initializeRendererHooks();
void shutdownRendererHooks();

bool should_render_window_original(PHLWINDOW window, PHLMONITOR monitor);
bool should_render_window_original(void* renderer, PHLWINDOW window, PHLMONITOR monitor);
bool render_workspace_original(
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
);
bool render_workspace_original(
    void* thisptr,
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& now,
    const CBox& geometry
);
bool render_window_original(
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    HTRenderPassMode mode,
    bool ignore_position,
    bool standalone
);
bool render_window_original(
    void* thisptr,
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    bool decorate,
    HTRenderPassMode mode,
    bool ignore_position,
    bool standalone
);
Vector2D monitor_position(PHLMONITOR monitor);
MONITORID monitor_id(PHLMONITOR monitor);
CBox monitor_logical_box(PHLMONITOR monitor);
float monitor_scale(PHLMONITOR monitor);
Vector2D monitor_transformed_size(PHLMONITOR monitor);
int monitor_transform(PHLMONITOR monitor);
PHLWORKSPACE active_monitor_workspace(PHLMONITOR monitor);
PHLMONITOR workspace_monitor(PHLWORKSPACE workspace);
MONITORID workspace_monitor_id(PHLWORKSPACE workspace);
bool workspace_render_visible(PHLWORKSPACE workspace);
WORKSPACEID workspace_id(PHLWORKSPACE workspace);
PHLWORKSPACE workspace_by_id(WORKSPACEID workspace_id);
PHLWORKSPACE window_workspace(PHLWINDOW window);
WORKSPACEID window_workspace_id(PHLWINDOW window);
PHLMONITOR window_monitor(PHLWINDOW window);
CBox window_main_surface_box(PHLWINDOW window);
Vector2D window_real_position(PHLWINDOW window);
Vector2D window_real_position_goal(PHLWINDOW window);
Vector2D window_real_size(PHLWINDOW window);
void set_window_real_position(PHLWINDOW window, const Vector2D& position);
void set_window_real_position_goal(PHLWINDOW window, const Vector2D& position);
void reset_window_workspace_move_alpha(PHLWINDOW window);
void warp_workspace_render_offset(PHLWORKSPACE workspace);
bool activate_monitor_workspace_user(PHLMONITOR monitor, PHLWORKSPACE workspace);
bool activate_monitor_workspace_internal(PHLMONITOR monitor, PHLWORKSPACE workspace);
bool restore_monitor_workspace(
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    bool use_change_workspace
);
void set_workspace_render_visibility(PHLWORKSPACE workspace, bool visible);
HTGradientValueData active_border_color();
HTGradientValueData inactive_border_color();
PHLWORKSPACE
resolve_workspace_target(PHLMONITOR monitor, WORKSPACEID workspace_id, bool create_if_missing);
bool move_workspace_to_monitor(
    PHLWORKSPACE workspace,
    PHLMONITOR monitor,
    bool no_warp_cursor = false
);
bool warp_pointer(const Vector2D& position);
void add_rect_pass(const CRectPassElement::SRectData& data);
void add_border_pass(const CBorderPassElement::SBorderData& data);
void add_renderer_hints_pass(const HTRenderModifData& data);
void damage_window(PHLWINDOW window);
void reset_overview_render_guard();
void begin_overview_render_pass();
void remove_clear_passes();
void finalize_overview_render_pass();

} // namespace HTCompat
