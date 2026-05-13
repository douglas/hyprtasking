#include "grid.hpp"

#include <cmath>
#include <format>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <string_view>

#include "../compat/renderer_compat.hpp"
#include "../compat/runtime_compat.hpp"
#include "../config.hpp"
#include "../globals.hpp"
#include "../logic/geometry_model.hpp"
#include "../overview.hpp"
#include "../render.hpp"
#include "../render_snapshot.hpp"
#include "../runtime_fail.hpp"
#include "../runtime_validation.hpp"
#include "../state_guards.hpp"
#include "../types.hpp"

using Hyprutils::Utils::CScopeGuard;

HTLayoutGrid::HTLayoutGrid(VIEWID new_view_id) : HTLayoutBase(new_view_id) {
    HTCompat::create_vector_animation({0, 0}, offset, "workspaces", AVARDAMAGE_NONE);
    HTCompat::create_float_animation(1.f, scale, "workspaces", AVARDAMAGE_NONE);

    init_position();
}

WORKSPACEID HTLayoutGrid::get_ws_id_in_direction(int x, int y, std::string& direction) {
    if (!HTRuntimeValidation::ensure_grid_gesture_or_disable("grid_config_validation"))
        return WORKSPACE_INVALID;
    const auto& config = HTConfig::runtime_config();

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

    if (config.grid_loop) {
        x = (x + config.grid_cols) % config.grid_cols;
        y = (y + config.grid_rows) % config.grid_rows;
    }
    return get_ws_id_from_xy(x, y);
}

void HTLayoutGrid::on_move_swipe(Vector2D delta) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;

    if (!HTRuntimeValidation::ensure_grid_gesture_or_disable("gesture_config_validation"))
        return;
    const auto& config = HTConfig::runtime_config();

    const CBox min_ws = calculate_ws_box(0, 0, HT_VIEW_CLOSED);
    const CBox max_ws =
        calculate_ws_box(config.grid_cols - 1, config.grid_rows - 1, HT_VIEW_CLOSED);

    Vector2D new_offset = offset->value() + delta / config.move_distance * max_ws.w;
    new_offset = new_offset.clamp(Vector2D {-max_ws.x, -max_ws.y}, Vector2D {-min_ws.x, -min_ws.y});

    offset->resetAllCallbacks();
    offset->setValueAndWarp(new_offset);
}

WORKSPACEID HTLayoutGrid::on_move_swipe_end() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return WORKSPACE_INVALID;

    build_overview_layout(HT_VIEW_CLOSED);
    WORKSPACEID closest = WORKSPACE_INVALID;
    double closest_dist = 1e9;
    for (const auto& [ws_id, box] : overview_layout) {
        const float dist_sq = offset->value().distanceSq(Vector2D {-box.box.x, -box.box.y});
        if (dist_sq < closest_dist) {
            closest_dist = dist_sq;
            closest = ws_id;
        }
    }
    return closest;
}

void HTLayoutGrid::close_open_lerp(float perc) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;
    const Vector2D transformed_size = HTCompat::monitor_transformed_size(monitor);

    double open_scale = calculate_ws_box(0, 0, HT_VIEW_OPENED).w / transformed_size.x; // 1 / ROWS
    Vector2D open_pos = {0, 0};

    build_overview_layout(HT_VIEW_CLOSED);
    double close_scale = 1.;
    const auto* active_layout = find_layout_workspace(HTCompat::workspace_id(active_workspace));
    if (active_layout == nullptr)
        return;
    Vector2D close_pos = -active_layout->box.pos();

    double new_scale = std::lerp(close_scale, open_scale, perc);
    Vector2D new_pos = Vector2D {
        std::lerp(close_pos.x, open_pos.x, perc),
        std::lerp(close_pos.y, open_pos.y, perc)
    };

    scale->resetAllCallbacks();
    offset->resetAllCallbacks();
    scale->setValueAndWarp(new_scale);
    offset->setValueAndWarp(new_pos);
}

void HTLayoutGrid::on_show(CallbackFun on_complete) {
    CScopeGuard x([this, &on_complete] {
        if (on_complete != nullptr)
            offset->setCallbackOnEnd(on_complete);
    });

    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;

    // Recalculate from active workspace so show animation starts from
    // the current workspace rather than stale state.
    init_position();
    close_render_workspace_id = WORKSPACE_INVALID;

    *scale = calculate_ws_box(0, 0, HT_VIEW_OPENED).w
        / HTCompat::monitor_transformed_size(monitor).x; // 1 / ROWS
    // Offset for the whole grid of workspaces
    *offset = {0, 0};
}

void HTLayoutGrid::on_hide(WORKSPACEID target_workspace_id, CallbackFun on_complete) {
    CScopeGuard x([this, &on_complete] {
        if (on_complete != nullptr)
            offset->setCallbackOnEnd(on_complete);
    });

    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    close_render_workspace_id = WORKSPACE_INVALID;

    build_overview_layout(HT_VIEW_CLOSED);
    *scale = 1.;

    WORKSPACEID close_workspace_id = target_workspace_id;
    if (close_workspace_id == WORKSPACE_INVALID) {
        const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
        if (active_workspace == nullptr)
            return;
        close_workspace_id = HTCompat::workspace_id(active_workspace);
    }

    const auto* target_layout = find_layout_workspace(close_workspace_id);
    if (target_layout == nullptr)
        return;
    close_render_workspace_id = close_workspace_id;
    *offset = -target_layout->box.pos();
}

void HTLayoutGrid::on_move(WORKSPACEID old_id, WORKSPACEID new_id, CallbackFun on_complete) {
    CScopeGuard x([this, &on_complete] {
        if (on_complete != nullptr)
            offset->setCallbackOnEnd(on_complete);
    });

    const PHTVIEW par_view = ht_manager->get_view_from_id(view_id);
    if (par_view == nullptr)
        return;

    // prevent the thing from animating
    const PHLWORKSPACE old_workspace = HTCompat::workspace_by_id(old_id);
    const PHLWORKSPACE new_workspace = HTCompat::workspace_by_id(new_id);
    if (old_workspace == nullptr || new_workspace == nullptr)
        return;
    HTCompat::warp_workspace_render_offset(old_workspace);
    HTCompat::warp_workspace_render_offset(new_workspace);

    build_overview_layout(HT_VIEW_CLOSED);
    *scale = 1.;
    // Target workspace to animate to
    const auto* target_layout = find_layout_workspace(new_id);
    if (target_layout == nullptr)
        return;
    *offset = -target_layout->box.pos();
}

bool HTLayoutGrid::should_render_window(PHLWINDOW window) {
    bool ori_result = HTLayoutBase::should_render_window(window);

    const PHLMONITOR monitor = get_monitor();
    if (window == nullptr || monitor == nullptr)
        return ori_result;

    if (ht_manager != nullptr && ht_manager->is_dragged_window(window))
        return false;

    PHLWORKSPACE workspace = HTCompat::window_workspace(window);
    if (workspace == nullptr)
        return false;

    CBox window_box = get_global_window_box(window, HTCompat::window_workspace_id(window));
    if (window_box.empty())
        return false;
    if (window_box.intersection(HTCompat::monitor_logical_box(monitor)).empty())
        return false;

    return ori_result;
}

float HTLayoutGrid::drag_window_scale() {
    return scale->value();
}

void HTLayoutGrid::init_position() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;

    build_overview_layout(HT_VIEW_CLOSED);
    const auto* active_layout = find_layout_workspace(HTCompat::workspace_id(active_workspace));
    if (active_layout == nullptr)
        return;
    close_render_workspace_id = WORKSPACE_INVALID;
    offset->setValueAndWarp(-active_layout->box.pos());
    scale->setValueAndWarp(1.f);
}

CBox HTLayoutGrid::calculate_ws_box(int x, int y, HTViewStage stage) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return {};

    const auto& config = HTConfig::runtime_config();
    const int ROWS = config.grid_rows;
    const int COLS = config.grid_cols;
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    const Vector2D transformed_size = HTCompat::monitor_transformed_size(monitor);
    constexpr float GAP_SIZE_LOGICAL = 8.F;
    const float gap_width = GAP_SIZE_LOGICAL * monitor_scale;
    const Vector2D gaps = {gap_width, gap_width};

    if (ROWS <= 0 || COLS <= 0) {
        return HTRuntimeFail::disable_and_return(
            "grid_config_validation",
            std::format("Invalid grid dimensions rows={} cols={}", ROWS, COLS),
            CBox {}
        );
    }
    if (!HTLogic::gridCellCount(ROWS, COLS, HTConfig::MAX_GRID_ROWS, HTConfig::MAX_GRID_COLS)
             .has_value()) {
        return HTRuntimeFail::disable_and_return(
            "grid_config_validation",
            std::format(
                "Grid dimensions exceed supported maximum rows={} cols={} max_rows={} max_cols={}",
                ROWS,
                COLS,
                HTConfig::MAX_GRID_ROWS,
                HTConfig::MAX_GRID_COLS
            ),
            CBox {}
        );
    }
    if (!HTLogic::isPositiveFinite(monitor_scale) || !HTLogic::isPositiveFinite(transformed_size.x)
        || !HTLogic::isPositiveFinite(transformed_size.y)) {
        return HTRuntimeFail::disable_and_return(
            "grid_geometry_validation",
            std::format(
                "Invalid monitor geometry scale={} size={}x{}",
                monitor_scale,
                transformed_size.x,
                transformed_size.y
            ),
            CBox {}
        );
    }

    if (gap_width > std::min(transformed_size.x, transformed_size.y) || gap_width < 0) {
        return HTRuntimeFail::disable_and_return(
            "grid_config_validation",
            std::format(
                "Gap size {} induces invalid render dimensions (monitor {}x{})",
                gap_width,
                transformed_size.x,
                transformed_size.y
            ),
            CBox {}
        );
    }

    double render_x = (transformed_size.x - gaps.x * (COLS + 1)) / COLS;
    double render_y = (transformed_size.y - gaps.y * (ROWS + 1)) / ROWS;
    const double mon_aspect = transformed_size.x / transformed_size.y;
    Vector2D start_offset {};

    // make correct aspect ratio
    if (render_y * mon_aspect > render_x) {
        start_offset.y = (render_y - render_x / mon_aspect) * ROWS / 2.f;
        render_y = render_x / mon_aspect;
    } else if (render_x / mon_aspect > render_y) {
        start_offset.x = (render_x - render_y * mon_aspect) * COLS / 2.f;
        render_x = render_y * mon_aspect;
    }

    float use_scale = scale->value();
    Vector2D use_offset = offset->value();
    if (stage == HT_VIEW_CLOSED) {
        use_scale = 1;
        use_offset = Vector2D {0, 0};
    } else if (stage == HT_VIEW_OPENED) {
        use_scale = render_x / transformed_size.x;
        use_offset = Vector2D {0, 0};
    }

    const Vector2D ws_sz = transformed_size * use_scale;
    return CBox {Vector2D {x, y} * (ws_sz + gaps) + gaps + use_offset + start_offset, ws_sz};
};

void HTLayoutGrid::build_overview_layout(HTViewStage stage) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;

    const auto& config = HTConfig::runtime_config();
    const int ROWS = config.grid_rows;
    const int COLS = config.grid_cols;

    overview_layout.clear();

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            const WORKSPACEID ws_id = (view_id * ROWS + y) * COLS + x + 1;
            const PHLWORKSPACE workspace = HTCompat::workspace_by_id(ws_id);
            if (workspace != nullptr && HTCompat::workspace_monitor_id(workspace) != view_id) {
                HTCompat::move_workspace_to_monitor(workspace, monitor);
            }
            const CBox ws_box = calculate_ws_box(x, y, stage);
            overview_layout[ws_id] = HTWorkspace {x, y, ws_box};
        }
    }
}

void HTLayoutGrid::render() {
    HTLayoutBase::begin_render();
    CScopeGuard x([this] { post_render(); });

    const PHTVIEW par_view = ht_manager->get_view_from_id(view_id);
    if (par_view == nullptr)
        return;
    const PHLMONITOR monitor = par_view->get_monitor();
    if (monitor == nullptr)
        return;

    const HTGradientValueData active_col = HTCompat::active_border_color();
    const HTGradientValueData inactive_col = HTCompat::inactive_border_color();

    constexpr float BORDERSIZE = 4.F;

    const auto render_scale = HTLogic::dragWindowScale(scale->value());
    if (!render_scale.has_value()) {
        HTRuntimeFail::disable(
            "grid_render_validation",
            std::format("Invalid render scale {}", scale->value())
        );
        return;
    }
    const auto render_snapshot = capture_render_snapshot(view_id, drag_window_scale());
    const auto time = render_snapshot.has_value() ? render_snapshot->time : Time::steadyNow();
    PHLWORKSPACE start_workspace = nullptr;
    if (par_view->closing && close_render_workspace_id != WORKSPACE_INVALID)
        start_workspace = HTCompat::workspace_by_id(close_render_workspace_id);
    if (start_workspace == nullptr && render_snapshot.has_value())
        start_workspace = render_snapshot->active_workspace;
    if (start_workspace == nullptr)
        start_workspace = HTCompat::active_monitor_workspace(monitor);
    const WORKSPACEID selected_workspace_id = par_view->visual_workspace_id(
        start_workspace == nullptr ? WORKSPACE_INVALID : HTCompat::workspace_id(start_workspace)
    );
    HTCompat::damage_monitor(monitor);
    const Vector2D transformed_size = HTCompat::monitor_transformed_size(monitor);
    if (!HTLogic::isPositiveFinite(transformed_size.x)
        || !HTLogic::isPositiveFinite(transformed_size.y)) {
        HTRuntimeFail::disable(
            "grid_render_validation",
            std::format("Invalid monitor render size {}x{}", transformed_size.x, transformed_size.y)
        );
        return;
    }
    CBox monitor_box = {{0, 0}, transformed_size};

    CRectPassElement::SRectData data;
    data.color = CHyprColor {0x000000FF}.stripA();
    data.box = monitor_box;
    HTCompat::add_rect_pass(data);

    // Do a dance with active workspaces: Hyprland will only properly render the
    // current active one so make the workspace active before rendering it, etc
    if (start_workspace == nullptr)
        return;

    HTScopedWorkspaceVisibility hide_start_workspace(start_workspace, false);

    build_overview_layout(HT_VIEW_ANIMATING);

    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    CBox global_mon_box = {monitor_pos, transformed_size};
    for (const auto& [ws_id, ws_layout] : overview_layout) {
        // Skip if the box is empty
        if (ws_layout.box.width < 0.01 || ws_layout.box.height < 0.01)
            continue;

        const PHLWORKSPACE workspace = HTCompat::workspace_by_id(ws_id);

        // renderModif translation used by renderWorkspace is weird so need
        // to scale the translation up as well. Geometry is also calculated from pixel size and not transformed size??
        CBox render_box = {{ws_layout.box.pos() / *render_scale}, ws_layout.box.size()};
        if (HTCompat::monitor_transform(monitor) % 2 == 1)
            std::swap(render_box.w, render_box.h);

        // render active one last
        if (workspace == start_workspace && start_workspace != nullptr)
            continue;

        CBox global_box = {ws_layout.box.pos() + monitor_pos, ws_layout.box.size()};
        if (global_box.expand(BORDERSIZE).intersection(global_mon_box).empty())
            continue;

        const HTGradientValueData border_col =
            selected_workspace_id == ws_id ? active_col : inactive_col;
        CBox border_box = ws_layout.box;

        CBorderPassElement::SBorderData data;
        data.box = border_box;
        data.grad1 = border_col;
        data.borderSize = BORDERSIZE;
        HTCompat::add_border_pass(data);

        render_workspace_with_optional_state(monitor, workspace, time, render_box);
    }

    hide_start_workspace.dismiss();
    HTCompat::set_workspace_render_visibility(start_workspace, true);

    // Render active workspace last so the dragging window is always on top when let go of
    if (const auto* start_layout = find_layout_workspace(HTCompat::workspace_id(start_workspace));
        start_layout != nullptr) {
        CBox ws_box = start_layout->box;
        // make sure box is not empty
        if (ws_box.width > 0.01 && ws_box.height > 0.01) {
            // renderModif translation used by renderWorkspace is weird so need
            // to scale the translation up as well. Geometry is also calculated from pixel size and not transformed size??
            CBox render_box = {{ws_box.pos() / *render_scale}, ws_box.size()};
            if (HTCompat::monitor_transform(monitor) % 2 == 1)
                std::swap(render_box.w, render_box.h);

            const HTGradientValueData border_col =
                selected_workspace_id == HTCompat::workspace_id(start_workspace) ? active_col
                                                                                 : inactive_col;
            CBox border_box = ws_box;

            CBorderPassElement::SBorderData data;
            data.box = border_box;
            data.grad1 = border_col;
            data.borderSize = BORDERSIZE;
            HTCompat::add_border_pass(data);

            render_workspace_with_optional_state(monitor, start_workspace, time, render_box);
        }
    }

    if (render_snapshot.has_value())
        render_dragged_window_snapshot(*render_snapshot);
}

void HTLayoutGrid::cancel_animation_callbacks() {
    close_render_workspace_id = WORKSPACE_INVALID;
    scale->resetAllCallbacks();
    offset->resetAllCallbacks();
}
