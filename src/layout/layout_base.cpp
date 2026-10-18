#include "layout_base.hpp"

#include <cmath>

#include "../compat/renderer_compat.hpp"
#include "../globals.hpp"
#include "../logic/geometry_model.hpp"
#include "../state_guards.hpp"

void HTLayoutBase::begin_render() {
    rendered_workspace_cells = false;
    HTCompat::begin_overview_render_pass();
}

void HTLayoutBase::render_workspace_with_optional_state(
    PHLMONITOR monitor,
    PHLWORKSPACE workspace,
    const Time::steady_tp& time,
    const CBox& render_box
) {
    // Hyprland renders wallpaper and layers for a null workspace as well.
    // Empty grid slots may not have a workspace until a window is moved there.
    HTScopedWorkspaceRender render_workspace(monitor, workspace);
    HTCompat::render_workspace_original(monitor, workspace, time, render_box);
    rendered_workspace_cells = true;
}

void HTLayoutBase::post_render() {
    // Pass elements run after the frame is built, so one sweep removes the
    // clear passes added by every rendered workspace cell.
    if (rendered_workspace_cells)
        HTCompat::remove_clear_passes();
    HTCompat::finalize_overview_render_pass();
}

PHLMONITOR HTLayoutBase::get_monitor() {
    const auto par_view = ht_manager->get_view_from_id(view_id);
    if (par_view == nullptr)
        return nullptr;
    return par_view->get_monitor();
}

const HTLayoutBase::HTWorkspace*
HTLayoutBase::find_layout_workspace(WORKSPACEID workspace_id) const {
    const auto it = overview_layout.find(workspace_id);
    if (it == overview_layout.end())
        return nullptr;

    return &it->second;
}

WORKSPACEID HTLayoutBase::get_ws_id_from_global(Vector2D pos) {
    const PHLMONITOR monitor = get_monitor();
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    if (monitor == nullptr || !HTLogic::isPositiveFinite(monitor_scale))
        return WORKSPACE_INVALID;

    if (!HTCompat::monitor_logical_box(monitor).containsPoint(pos))
        return WORKSPACE_INVALID;

    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    Vector2D relative_pos = (pos - monitor_pos) * monitor_scale;
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

CBox HTLayoutBase::get_global_window_box(PHLWINDOW window, PHLWORKSPACE workspace) {
    if (window == nullptr)
        return {};

    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return {};

    if (workspace == nullptr || HTCompat::workspace_monitor(workspace) != monitor)
        return {};
    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);

    const CBox ws_window_box = HTCompat::window_main_surface_box(window);

    const Vector2D top_left = local_ws_unscaled_to_global(
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

// Where workspace_id is drawn right now; nullopt without a usable cell.
std::optional<HTLogic::CellMapping> HTLayoutBase::cell_mapping(WORKSPACEID workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return std::nullopt;
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    if (!HTLogic::isPositiveFinite(monitor_scale))
        return std::nullopt;

    const auto* layout_workspace = find_layout_workspace(workspace_id);
    if (layout_workspace == nullptr || layout_workspace->box.empty())
        return std::nullopt;
    const CBox& box = layout_workspace->box;
    const auto width_scale =
        HTLogic::workspaceWidthScale(box.w, HTCompat::monitor_transformed_size(monitor).x);
    if (!width_scale.has_value())
        return std::nullopt;

    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    return HTLogic::CellMapping {
        .cell = {.x = box.x, .y = box.y},
        .width_scale = *width_scale,
        .monitor_scale = monitor_scale,
        .monitor = {.x = monitor_pos.x, .y = monitor_pos.y},
    };
}

Vector2D HTLayoutBase::global_to_local_ws_unscaled(Vector2D pos, WORKSPACEID workspace_id) {
    const auto mapping = cell_mapping(workspace_id);
    if (!mapping.has_value())
        return {};

    const auto local = HTLogic::globalToWorkspace(*mapping, {.x = pos.x, .y = pos.y});
    if (!HTLogic::isFinitePoint(local.x, local.y))
        return {};
    return {local.x, local.y};
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
    const auto mapping = cell_mapping(workspace_id);
    if (!mapping.has_value())
        return {};

    const auto global = HTLogic::workspaceToGlobal(*mapping, {.x = pos.x, .y = pos.y});
    if (!HTLogic::isFinitePoint(global.x, global.y))
        return {};
    return {global.x, global.y};
}

Vector2D HTLayoutBase::local_ws_scaled_to_global(Vector2D pos, WORKSPACEID workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    if (monitor == nullptr || !HTLogic::isPositiveFinite(monitor_scale))
        return {};

    pos /= monitor_scale;
    return local_ws_unscaled_to_global(pos, workspace_id);
}
