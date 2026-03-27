#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>

#include "../compat/renderer_compat.hpp"
#include "../globals.hpp"
#include "../logic/geometry_model.hpp"
#include "layout_base.hpp"

HTLayoutBase::HTLayoutBase(VIEWID new_view_id) : view_id(new_view_id) {
    ;
}

void HTLayoutBase::on_move_swipe(Vector2D delta) {
    ;
}

WORKSPACEID HTLayoutBase::on_move_swipe_end() {
    return WORKSPACE_INVALID;
}

WORKSPACEID HTLayoutBase::get_ws_id_in_direction(int x, int y, std::string& direction) {
    if (direction == "up") {
        y--;
    } else if (direction == "down") {
        y++;
    } else if (direction == "right") {
        x++;
    } else if (direction == "left") {
        x--;
    } else {
        return WORKSPACE_INVALID;
    }
    return get_ws_id_from_xy(x, y);
}

bool HTLayoutBase::on_mouse_axis(double delta) {
    return false;
}

bool HTLayoutBase::should_manage_mouse() {
    return true;
}

bool HTLayoutBase::should_render_window(PHLWINDOW window) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr || window == nullptr)
        return false;

    return HTCompat::should_render_window_original(g_pHyprRenderer.get(), window, monitor);
}

float HTLayoutBase::drag_window_scale() {
    return 1.f;
}

void HTLayoutBase::init_position() {
    ;
}

void HTLayoutBase::build_overview_layout(HTViewStage stage) {
    ;
}

void HTLayoutBase::render() {
    HTCompat::begin_overview_render_pass();
}

void HTLayoutBase::cancel_animation_callbacks() {
    ;
}

void HTLayoutBase::post_render() {
    HTCompat::finalize_overview_render_pass();
}

PHLMONITOR HTLayoutBase::get_monitor() {
    const auto par_view = ht_manager->get_view_from_id(view_id);
    if (par_view == nullptr)
        return nullptr;
    return par_view->get_monitor();
}

const HTLayoutBase::HTWorkspace* HTLayoutBase::find_layout_workspace(WORKSPACEID workspace_id) const {
    const auto it = overview_layout.find(workspace_id);
    if (it == overview_layout.end())
        return nullptr;

    return &it->second;
}

WORKSPACEID HTLayoutBase::get_ws_id_from_global(Vector2D pos) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr || !HTLogic::isPositiveFinite(monitor->m_scale))
        return WORKSPACE_INVALID;

    if (!monitor->logicalBox().containsPoint(pos))
        return WORKSPACE_INVALID;

    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    Vector2D relative_pos = (pos - monitor_pos) * monitor->m_scale;
    for (const auto& [id, layout] : overview_layout)
        if (layout.box.containsPoint(relative_pos))
            return id;

    return WORKSPACE_INVALID;
}

WORKSPACEID HTLayoutBase::get_ws_id_from_xy(int x, int y) {
    for (const auto& [id, layout] : overview_layout)
        if (layout.x == x && layout.y == y)
            return id;

    return WORKSPACE_INVALID;
}

CBox HTLayoutBase::get_global_window_box(PHLWINDOW window, WORKSPACEID workspace_id) {
    if (window == nullptr)
        return {};

    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return {};

    const PHLWORKSPACE workspace = g_pCompositor->getWorkspaceByID(workspace_id);
    if (workspace == nullptr || HTCompat::workspace_monitor(workspace) != monitor)
        return {};
    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);

    const CBox ws_window_box = window->getWindowMainSurfaceBox();

    const Vector2D top_left =
        local_ws_unscaled_to_global(
            ws_window_box.pos() - monitor_pos,
            HTCompat::workspace_id(workspace)
        );
    const Vector2D bottom_right = local_ws_unscaled_to_global(
        ws_window_box.pos() + ws_window_box.size() - monitor_pos,
        HTCompat::workspace_id(workspace)
    );
    if (!HTLogic::isFinitePoint(top_left.x, top_left.y)
        || !HTLogic::isFinitePoint(bottom_right.x, bottom_right.y))
        return {};

    return {top_left, bottom_right - top_left};
}

CBox HTLayoutBase::get_global_ws_box(WORKSPACEID workspace_id) {
    const auto* layout_workspace = find_layout_workspace(workspace_id);
    if (layout_workspace == nullptr)
        return {};

    const CBox scaled_ws_box = layout_workspace->box;
    const Vector2D top_left = local_ws_scaled_to_global(scaled_ws_box.pos(), workspace_id);
    const Vector2D bottom_right =
        local_ws_scaled_to_global(scaled_ws_box.pos() + scaled_ws_box.size(), workspace_id);
    if (!HTLogic::isFinitePoint(top_left.x, top_left.y)
        || !HTLogic::isFinitePoint(bottom_right.x, bottom_right.y))
        return {};
    return {top_left, bottom_right - top_left};
}

Vector2D HTLayoutBase::global_to_local_ws_unscaled(Vector2D pos, WORKSPACEID workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return {};
    if (!HTLogic::isPositiveFinite(monitor->m_scale))
        return {};

    const auto* layout_workspace = find_layout_workspace(workspace_id);
    if (layout_workspace == nullptr)
        return {};

    CBox workspace_box = layout_workspace->box;
    if (workspace_box.empty())
        return {};
    const auto width_scale =
        HTLogic::workspaceWidthScale(workspace_box.w, monitor->m_transformedSize.x);
    if (!width_scale.has_value())
        return {};
    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);

    pos -= monitor_pos;
    pos *= monitor->m_scale;
    pos -= workspace_box.pos();
    pos /= monitor->m_scale;
    pos /= *width_scale;
    if (!HTLogic::isFinitePoint(pos.x, pos.y))
        return {};
    return pos;
}

Vector2D HTLayoutBase::global_to_local_ws_scaled(Vector2D pos, WORKSPACEID workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr || !HTLogic::isPositiveFinite(monitor->m_scale))
        return {};

    pos = global_to_local_ws_unscaled(pos, workspace_id);
    pos *= monitor->m_scale;
    return pos;
}

std::optional<Vector2D>
HTLayoutBase::global_to_workspace_monitor_coords(Vector2D pos, WORKSPACEID workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return std::nullopt;

    const Vector2D local_coords = global_to_local_ws_unscaled(pos, workspace_id);
    const Vector2D monitor_coords = local_coords + HTCompat::monitor_position(monitor);
    if (!std::isfinite(monitor_coords.x) || !std::isfinite(monitor_coords.y))
        return std::nullopt;

    return monitor_coords;
}

Vector2D HTLayoutBase::local_ws_unscaled_to_global(Vector2D pos, WORKSPACEID workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return {};
    if (!HTLogic::isPositiveFinite(monitor->m_scale))
        return {};

    const auto* layout_workspace = find_layout_workspace(workspace_id);
    if (layout_workspace == nullptr)
        return {};

    CBox workspace_box = layout_workspace->box;
    if (workspace_box.empty())
        return {};
    const auto width_scale =
        HTLogic::workspaceWidthScale(workspace_box.w, monitor->m_transformedSize.x);
    if (!width_scale.has_value())
        return {};
    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);

    pos *= *width_scale;
    pos *= monitor->m_scale;
    pos += workspace_box.pos();
    pos /= monitor->m_scale;
    pos += monitor_pos;
    if (!HTLogic::isFinitePoint(pos.x, pos.y))
        return {};
    return pos;
}

Vector2D HTLayoutBase::local_ws_scaled_to_global(Vector2D pos, WORKSPACEID workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr || !HTLogic::isPositiveFinite(monitor->m_scale))
        return {};

    pos /= monitor->m_scale;
    return local_ws_unscaled_to_global(pos, workspace_id);
}
