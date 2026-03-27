#include "grid.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/helpers/AnimatedVariable.hpp>
#include <hyprland/src/managers/animation/DesktopAnimationManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprutils/math/Vector2D.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "../compat/renderer_compat.hpp"
#include "../compat/runtime_compat.hpp"
#include "../config.hpp"
#include "../globals.hpp"
#include "../overview.hpp"
#include "../render.hpp"
#include "../render_snapshot.hpp"
#include "../state_guards.hpp"
#include "../types.hpp"
#include "src/layout/target/Target.hpp"

using Hyprutils::Utils::CScopeGuard;

HTLayoutGrid::HTLayoutGrid(VIEWID new_view_id) : HTLayoutBase(new_view_id) {
    HTCompat::create_vector_animation(
        {0, 0},
        offset,
        "workspaces",
        AVARDAMAGE_NONE
    );
    HTCompat::create_float_animation(
        1.f,
        scale,
        "workspaces",
        AVARDAMAGE_NONE
    );

    init_position();
}

std::string HTLayoutGrid::layout_name() {
    return "grid";
}

WORKSPACEID HTLayoutGrid::get_ws_id_in_direction(int x, int y, std::string& direction) {
    const int LOOP = HTConfig::value<Hyprlang::INT>("grid:loop");
    const int ROWS = HTConfig::value<Hyprlang::INT>("grid:rows");
    const int COLS = HTConfig::value<Hyprlang::INT>("grid:cols");

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

    if (LOOP) {
        x = (x + COLS) % COLS;
        y = (y + ROWS) % ROWS;
    }
    return get_ws_id_from_xy(x, y);
}

void HTLayoutGrid::on_move_swipe(Vector2D delta) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;

    const float MOVE_DISTANCE = HTConfig::value<Hyprlang::FLOAT>("gestures:move_distance");
    const int ROWS = HTConfig::value<Hyprlang::INT>("grid:rows");
    const int COLS = HTConfig::value<Hyprlang::INT>("grid:cols");
    const CBox min_ws = calculate_ws_box(0, 0, HT_VIEW_CLOSED);
    const CBox max_ws = calculate_ws_box(COLS - 1, ROWS - 1, HT_VIEW_CLOSED);

    Vector2D new_offset = offset->value() + delta / MOVE_DISTANCE * max_ws.w;
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

    double open_scale =
        calculate_ws_box(0, 0, HT_VIEW_OPENED).w / transformed_size.x; // 1 / ROWS
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

    // HACK: This is needed to recalculate the position of the current workspace,
    // so we don't start animating from an inactive workspace
    init_position();

    *scale = calculate_ws_box(0, 0, HT_VIEW_OPENED).w
        / HTCompat::monitor_transformed_size(monitor).x; // 1 / ROWS
    // Offset for the whole grid of workspaces
    *offset = {0, 0};
}

void HTLayoutGrid::on_hide(CallbackFun on_complete) {
    CScopeGuard x([this, &on_complete] {
        if (on_complete != nullptr)
            offset->setCallbackOnEnd(on_complete);
    });

    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;
    const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(monitor);
    if (active_workspace == nullptr)
        return;

    build_overview_layout(HT_VIEW_CLOSED);
    *scale = 1.;
    // End workspace to end up on
    const auto* active_layout = find_layout_workspace(HTCompat::workspace_id(active_workspace));
    if (active_layout == nullptr)
        return;
    *offset = -active_layout->box.pos();
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

    CBox window_box = get_global_window_box(window, window->workspaceID());
    if (window_box.empty())
        return false;
    if (window_box.intersection(monitor->logicalBox()).empty())
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
    offset->setValueAndWarp(-active_layout->box.pos());
    scale->setValueAndWarp(1.f);
}

CBox HTLayoutGrid::calculate_ws_box(int x, int y, HTViewStage stage) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return {};

    const int ROWS = HTConfig::value<Hyprlang::INT>("grid:rows");
    const int COLS = HTConfig::value<Hyprlang::INT>("grid:cols");
    const int GAPS_USE_ASPECT_RATIO = HTConfig::value<Hyprlang::INT>("grid:gaps_use_aspect_ratio");
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    const Vector2D transformed_size = HTCompat::monitor_transformed_size(monitor);
    const float GAP_SIZE = HTConfig::value<Hyprlang::FLOAT>("gap_size") * monitor_scale;
    const Vector2D gaps = {
        GAP_SIZE,
        GAPS_USE_ASPECT_RATIO
            ? GAP_SIZE * transformed_size.y / transformed_size.x
            : GAP_SIZE
    };

    if (GAP_SIZE > std::min(transformed_size.x, transformed_size.y)
        || GAP_SIZE < 0)
        fail_exit("Gap size {} induces invalid render dimensions", GAP_SIZE);

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

    const int ROWS = HTConfig::value<Hyprlang::INT>("grid:rows");
    const int COLS = HTConfig::value<Hyprlang::INT>("grid:cols");

    HTScopedMonitorFocus restore_focus(monitor);

    overview_layout.clear();

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            const WORKSPACEID ws_id = (view_id * ROWS + y) * COLS + x + 1;
            const PHLWORKSPACE workspace = HTCompat::workspace_by_id(ws_id);
            if (workspace != nullptr && workspace->monitorID() != view_id) {
                HTCompat::move_workspace_to_monitor(workspace, monitor);
            }
            const CBox ws_box = calculate_ws_box(x, y, stage);
            overview_layout[ws_id] = HTWorkspace {x, y, ws_box};
        }
    }

}

void HTLayoutGrid::render() {
    HTLayoutBase::render();
    CScopeGuard x([this] { post_render(); });

    const PHTVIEW par_view = ht_manager->get_view_from_id(view_id);
    if (par_view == nullptr)
        return;
    const PHLMONITOR monitor = par_view->get_monitor();
    if (monitor == nullptr)
        return;

    static auto PACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.active_border");
    static auto PINACTIVECOL = CConfigValue<Hyprlang::CUSTOMTYPE>("general:col.inactive_border");

    auto* const ACTIVECOL = (CGradientValueData*)(PACTIVECOL.ptr())->getData();
    auto* const INACTIVECOL = (CGradientValueData*)(PINACTIVECOL.ptr())->getData();

    const float BORDERSIZE = HTConfig::value<Hyprlang::FLOAT>("border_size");

    const auto render_snapshot = capture_render_snapshot(view_id, drag_window_scale());
    const auto time = render_snapshot.has_value() ? render_snapshot->time : Time::steadyNow();
    const PHLWORKSPACE start_workspace = render_snapshot.has_value()
        ? render_snapshot->active_workspace
        : HTCompat::active_monitor_workspace(monitor);


    HTCompat::damage_monitor(monitor);
    HTCompat::set_current_monitor_blur_should_render(true);
    const Vector2D transformed_size = HTCompat::monitor_transformed_size(monitor);
    CBox monitor_box = {{0, 0}, transformed_size};

    CRectPassElement::SRectData data;
    data.color = CHyprColor {HTConfig::value<Hyprlang::INT>("bg_color")}.stripA();
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

        // Could be nullptr, in which we render only layers
        const PHLWORKSPACE workspace = HTCompat::workspace_by_id(ws_id);

        // renderModif translation used by renderWorkspace is weird so need
        // to scale the translation up as well. Geometry is also calculated from pixel size and not transformed size??
        CBox render_box = {{ws_layout.box.pos() / scale->value()}, ws_layout.box.size()};
        if (HTCompat::monitor_transform(monitor) % 2 == 1)
            std::swap(render_box.w, render_box.h);

        // render active one last
        if (workspace == start_workspace && start_workspace != nullptr)
            continue;

        CBox global_box = {ws_layout.box.pos() + monitor_pos, ws_layout.box.size()};
        if (global_box.expand(BORDERSIZE).intersection(global_mon_box).empty())
            continue;

        const CGradientValueData border_col =
            HTCompat::workspace_id(start_workspace) == ws_id ? *ACTIVECOL : *INACTIVECOL;
        CBox border_box = ws_layout.box;

        CBorderPassElement::SBorderData data;
        data.box = border_box;
        data.grad1 = border_col;
        data.borderSize = BORDERSIZE;
        HTCompat::add_border_pass(data);

        if (workspace != nullptr) {
            HTScopedWorkspaceRender render_workspace(monitor, workspace);
            HTCompat::render_workspace_original(
                monitor,
                workspace,
                time,
                render_box
            );
            HTCompat::remove_clear_passes();
        } else {
            // If pWorkspace is null, then just render the layers
            HTCompat::render_workspace_original(
                monitor,
                workspace,
                time,
                render_box
            );
            HTCompat::remove_clear_passes();
        }
    }

    hide_start_workspace.dismiss();
    HTCompat::set_workspace_render_visibility(start_workspace, true);

    // Render active workspace last so the dragging window is always on top when let go of
    if (const auto* start_layout =
            find_layout_workspace(HTCompat::workspace_id(start_workspace));
        start_layout != nullptr) {
        CBox ws_box = start_layout->box;
        // make sure box is not empty
        if (ws_box.width > 0.01 && ws_box.height > 0.01) {
            // renderModif translation used by renderWorkspace is weird so need
            // to scale the translation up as well. Geometry is also calculated from pixel size and not transformed size??
            CBox render_box = {{ws_box.pos() / scale->value()}, ws_box.size()};
            if (HTCompat::monitor_transform(monitor) % 2 == 1)
                std::swap(render_box.w, render_box.h);

            const CGradientValueData border_col = *ACTIVECOL;
            CBox border_box = ws_box;

            CBorderPassElement::SBorderData data;
            data.box = border_box;
            data.grad1 = border_col;
            data.borderSize = BORDERSIZE;
            HTCompat::add_border_pass(data);

            HTCompat::render_workspace_original(
                monitor,
                start_workspace,
                time,
                render_box
            );
            HTCompat::remove_clear_passes();
        }
    }

    if (render_snapshot.has_value())
        render_dragged_window_snapshot(*render_snapshot);
}

void HTLayoutGrid::cancel_animation_callbacks() {
    scale->resetAllCallbacks();
    offset->resetAllCallbacks();
}
