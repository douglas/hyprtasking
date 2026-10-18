#include "grid.hpp"

#include <cmath>
#include <format>
#include <hyprutils/math/Vector2D.hpp>
#include <optional>
#include <stdexcept>
#include <string_view>

#include "../compat/renderer_compat.hpp"
#include "../compat/runtime_compat.hpp"
#include "../config.hpp"
#include "../globals.hpp"
#include "../logic/logic.hpp"
#include "../overview.hpp"
#include "../plugin/guards.hpp"
#include "../scope_guard.hpp"
#include "../state_guards.hpp"
#include "../types.hpp"

namespace {

// Geometry can be briefly invalid (e.g. mid-modeset). Skip the work and log
// each distinct problem once instead of disabling the runtime.
void warn_invalid_geometry(std::string_view source, const std::string& reason) {
    static std::string last_reason;
    if (reason == last_reason)
        return;

    last_reason = reason;
    Log::logger->log(Log::WARN, "[Hyprtasking] {}: {}", source, reason);
}

struct DraggedWindowBox {
    PHLWINDOW window;
    CBox box;
};

// The dragged window scaled around the cursor, when it shows on monitor.
std::optional<DraggedWindowBox> dragged_window_box(PHLMONITOR monitor, float scale) {
    const PHLWINDOW window = ht_manager->get_dragged_window();
    if (window == nullptr)
        return std::nullopt;

    const Vector2D mouse_coords = HTCompat::mouse_coords();
    if (!HTLogic::isFinitePoint(mouse_coords.x, mouse_coords.y))
        return std::nullopt;

    const CBox box = HTCompat::window_main_surface_box(window)
                         .translate(-mouse_coords)
                         .scale(scale)
                         .translate(mouse_coords);
    if (box.empty() || !HTLogic::isFinitePoint(box.x, box.y)
        || !HTLogic::isFinitePoint(box.w, box.h))
        return std::nullopt;
    if (box.intersection(HTCompat::monitor_logical_box(monitor)).empty())
        return std::nullopt;

    return DraggedWindowBox {.window = window, .box = box};
}

// Note: box is relative to (0, 0), not monitor
void render_window_at_box(
    PHLWINDOW window,
    PHLMONITOR monitor,
    const Time::steady_tp& time,
    CBox box
) {
    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    box.x -= monitor_pos.x;
    box.y -= monitor_pos.y;

    const Vector2D window_size = HTCompat::window_real_size(window);
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    if (!HTLogic::isPositiveFinite(monitor_scale))
        return;

    const auto scale = HTLogic::windowRenderScale(box.w, window_size.x);
    if (!scale.has_value())
        return;

    const Vector2D transform =
        (monitor_pos - HTCompat::window_real_position(window) + box.pos() / *scale) * monitor_scale;
    if (!std::isfinite(transform.x) || !std::isfinite(transform.y))
        return;

    HTRenderModifData data {};
    data.modifs.push_back({HTRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, transform});
    data.modifs.push_back({HTRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, *scale});
    HTCompat::add_renderer_hints_pass(data);
    HTScopeGuard reset_hints([] { HTCompat::add_renderer_hints_pass(HTRenderModifData {}); });

    HTCompat::render_window_original(window, monitor, time, true, HT_RENDER_PASS_MAIN, false, true);
}

} // namespace

HTLayoutGrid::HTLayoutGrid(VIEWID new_view_id) : view_id(new_view_id) {
    if (!HTCompat::create_vector_animation({0, 0}, offset, "workspaces", AVARDAMAGE_NONE)
        || !HTCompat::create_float_animation(1.f, scale, "workspaces", AVARDAMAGE_NONE))
        throw std::runtime_error("failed creating overview animation variables");

    init_position();
}

WORKSPACEID HTLayoutGrid::get_ws_id_in_direction(int x, int y, std::string_view direction) {
    const auto& config = HTConfig::runtime_config();

    const auto target = HTLogic::moveGridInDirection(
        x,
        y,
        direction,
        config.grid_rows,
        config.grid_cols,
        config.grid_loop
    );
    if (!target.has_value())
        return WORKSPACE_INVALID;
    return get_ws_id_from_xy(target->x, target->y);
}

void HTLayoutGrid::on_move_swipe(Vector2D delta) {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;

    const auto& config = HTConfig::runtime_config();

    const CBox min_ws = calculate_ws_box(0, 0, HT_VIEW_CLOSED);
    const CBox max_ws =
        calculate_ws_box(config.grid_cols - 1, config.grid_rows - 1, HT_VIEW_CLOSED);

    Vector2D new_offset = offset->value() + delta / config.move_distance * max_ws.w;
    new_offset = new_offset.clamp(Vector2D {-max_ws.x, -max_ws.y}, Vector2D {-min_ws.x, -min_ws.y});

    offset->resetAllCallbacks();
    offset->setValueAndWarp(new_offset);
    request_redraw();
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

    const auto geometry = grid_geometry();
    build_overview_layout(HT_VIEW_CLOSED);
    const auto* active_layout = find_layout_workspace(HTCompat::workspace_id(active_workspace));
    if (!geometry.has_value() || active_layout == nullptr)
        return;

    const double open_scale = geometry->fit.opened_scale;
    const Vector2D open_pos = {0, 0};
    const double close_scale = 1.;
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

// End callbacks run from Hyprland's animation tick, or right away from the
// caller's scope guard destructor, so nothing may escape them. They can also
// reset this variable's callbacks (e.g. by disabling the runtime), which frees
// the closure mid-call, so run a local copy.
void HTLayoutGrid::set_offset_end_callback(CallbackFun on_complete) {
    offset->setCallbackOnEnd([on_complete = std::move(on_complete)](auto var) {
        const CallbackFun callback = on_complete;
        HTPlugin::guardedCallback("animation_end", [&callback, &var] { callback(var); });
    });
}

void HTLayoutGrid::on_show(CallbackFun on_complete) {
    HTScopeGuard x([this, &on_complete] {
        if (on_complete != nullptr)
            set_offset_end_callback(on_complete);
    });

    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;

    // Recalculate from active workspace so show animation starts from
    // the current workspace rather than stale state.
    place_workspaces();
    init_position();
    close_render_workspace_id = WORKSPACE_INVALID;

    const auto geometry = grid_geometry();
    *scale = geometry.has_value() ? geometry->fit.opened_scale : 0.0;
    // Offset for the whole grid of workspaces
    *offset = {0, 0};
}

void HTLayoutGrid::on_hide(WORKSPACEID target_workspace_id, CallbackFun on_complete) {
    HTScopeGuard x([this, &on_complete] {
        if (on_complete != nullptr)
            set_offset_end_callback(on_complete);
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
    HTScopeGuard x([this, &on_complete] {
        if (on_complete != nullptr)
            set_offset_end_callback(on_complete);
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

// original_result is Hyprland's own answer for this window on this view's monitor.
bool HTLayoutGrid::should_render_window(PHLWINDOW window, bool original_result) {
    const PHLMONITOR monitor = get_monitor();
    if (window == nullptr || monitor == nullptr)
        return original_result;

    if (ht_manager->is_dragged_window(window))
        return false;

    PHLWORKSPACE workspace = HTCompat::window_workspace(window);
    if (workspace == nullptr)
        return false;

    if (HTCompat::workspace_monitor(workspace) != monitor)
        return false;

    // The window's box where its workspace cell draws it.
    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    const CBox ws_window_box = HTCompat::window_main_surface_box(window);
    const WORKSPACEID workspace_id = HTCompat::workspace_id(workspace);
    const Vector2D top_left =
        local_ws_unscaled_to_global(ws_window_box.pos() - monitor_pos, workspace_id);
    const Vector2D bottom_right = local_ws_unscaled_to_global(
        ws_window_box.pos() + ws_window_box.size() - monitor_pos,
        workspace_id
    );
    if (!HTLogic::isFinitePoint(top_left.x, top_left.y)
        || !HTLogic::isFinitePoint(bottom_right.x, bottom_right.y))
        return false;

    const CBox window_box = {top_left, bottom_right - top_left};
    if (window_box.empty())
        return false;
    if (window_box.intersection(HTCompat::monitor_logical_box(monitor)).empty())
        return false;

    return original_result;
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

std::optional<HTLayoutGrid::GridGeometry> HTLayoutGrid::grid_geometry() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return std::nullopt;

    const auto& config = HTConfig::runtime_config();
    const int ROWS = config.grid_rows;
    const int COLS = config.grid_cols;
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    const Vector2D transformed_size = HTCompat::monitor_transformed_size(monitor);
    constexpr float GAP_SIZE_LOGICAL = 8.F;
    const float gap_width = GAP_SIZE_LOGICAL * monitor_scale;

    const auto fit =
        HTLogic::fitGrid(transformed_size.x, transformed_size.y, ROWS, COLS, gap_width);
    if (!HTLogic::isPositiveFinite(monitor_scale) || !fit.has_value()) {
        warn_invalid_geometry(
            "grid_geometry_validation",
            std::format(
                "Invalid monitor geometry scale={} size={}x{} for a {}x{} grid",
                monitor_scale,
                transformed_size.x,
                transformed_size.y,
                ROWS,
                COLS
            )
        );
        return std::nullopt;
    }

    return GridGeometry {.transformed_size = transformed_size, .fit = *fit};
}

CBox HTLayoutGrid::cell_box(const GridGeometry& geometry, int x, int y, HTViewStage stage) const {
    double use_scale = scale->value();
    Vector2D use_offset = offset->value();
    if (stage == HT_VIEW_CLOSED) {
        use_scale = 1;
        use_offset = Vector2D {0, 0};
    } else if (stage == HT_VIEW_OPENED) {
        use_scale = geometry.fit.opened_scale;
        use_offset = Vector2D {0, 0};
    }

    const auto box = HTLogic::gridCellBox(
        geometry.fit,
        {.x = geometry.transformed_size.x, .y = geometry.transformed_size.y},
        x,
        y,
        use_scale,
        {.x = use_offset.x, .y = use_offset.y}
    );
    return CBox {box.x, box.y, box.w, box.h};
}

CBox HTLayoutGrid::calculate_ws_box(int x, int y, HTViewStage stage) {
    const auto geometry = grid_geometry();
    if (!geometry.has_value())
        return {};

    return cell_box(*geometry, x, y, stage);
}

void HTLayoutGrid::set_grid_block(int block) {
    grid_block = block;
}

// Pull this monitor's block of workspaces onto it. Must not run from render:
// moving workspaces changes other monitors' state mid-frame.
void HTLayoutGrid::place_workspaces() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr || grid_block < 0)
        return;

    const auto& config = HTConfig::runtime_config();
    const int cell_count = config.grid_rows * config.grid_cols;
    for (int cell = 0; cell < cell_count; cell++) {
        const WORKSPACEID ws_id = HTLogic::gridWorkspaceID(
            grid_block,
            config.grid_rows,
            config.grid_cols,
            cell % config.grid_cols,
            cell / config.grid_cols
        );
        const PHLWORKSPACE workspace = HTCompat::workspace_by_id(ws_id);
        if (workspace != nullptr && HTCompat::workspace_monitor_id(workspace) != view_id)
            HTCompat::move_workspace_to_monitor(workspace, monitor, true);
    }
}

// Runs every frame while the overview renders: geometry is computed once, the
// map is only rebuilt when the set of workspace ids changes, and boxes are only
// recomputed when one of their inputs changed.
void HTLayoutGrid::build_overview_layout(HTViewStage stage) {
    const auto geometry = grid_block < 0 ? std::nullopt : grid_geometry();
    if (!geometry.has_value()) {
        overview_layout.clear();
        layout_inputs.reset();
        return;
    }

    const auto& config = HTConfig::runtime_config();
    const int ROWS = config.grid_rows;
    const int COLS = config.grid_cols;
    const LayoutShape shape {.block = grid_block, .rows = ROWS, .cols = COLS};
    const LayoutInputs inputs {
        .shape = shape,
        .stage = stage,
        .scale = scale->value(),
        .offset = offset->value(),
        .transformed_size = geometry->transformed_size,
        .gap = geometry->fit.gap,
    };
    if (layout_inputs == inputs)
        return;

    if (!layout_inputs.has_value() || layout_inputs->shape != shape) {
        overview_layout.clear();
        overview_layout.reserve(static_cast<size_t>(ROWS) * static_cast<size_t>(COLS));
    }
    layout_inputs = inputs;

    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            const WORKSPACEID ws_id = HTLogic::gridWorkspaceID(grid_block, ROWS, COLS, x, y);
            overview_layout[ws_id] = HTWorkspace {x, y, cell_box(*geometry, x, y, stage)};
        }
    }
}

void HTLayoutGrid::render() {
    bool rendered_workspace_cells = false;
    HTCompat::begin_overview_render_pass();
    HTScopeGuard finish_render([&rendered_workspace_cells] {
        // Pass elements run after the frame is built, so one sweep removes the
        // clear passes added by every rendered workspace cell.
        if (rendered_workspace_cells)
            HTCompat::remove_clear_passes();
        HTCompat::finalize_overview_render_pass();
    });

    const PHTVIEW par_view = ht_manager->get_view_from_id(view_id);
    if (par_view == nullptr)
        return;
    const PHLMONITOR monitor = par_view->get_monitor();
    if (monitor == nullptr)
        return;

    const HTGradientValueData& active_col = HTCompat::active_border_color();
    const HTGradientValueData& inactive_col = HTCompat::inactive_border_color();

    constexpr float BORDERSIZE = 4.F;

    const float render_scale = scale->value();
    if (!HTLogic::isPositiveFinite(render_scale)) {
        warn_invalid_geometry(
            "grid_render_validation",
            std::format("Invalid render scale {}", scale->value())
        );
        return;
    }
    const auto dragged = dragged_window_box(monitor, render_scale);
    const auto time = Time::steadyNow();
    PHLWORKSPACE start_workspace = nullptr;
    if (par_view->closing && close_render_workspace_id != WORKSPACE_INVALID)
        start_workspace = HTCompat::workspace_by_id(close_render_workspace_id);
    if (start_workspace == nullptr)
        start_workspace = HTCompat::active_monitor_workspace(monitor);
    const WORKSPACEID selected_workspace_id = par_view->visual_workspace_id(
        start_workspace == nullptr ? WORKSPACE_INVALID : HTCompat::workspace_id(start_workspace)
    );
    // Hyprland does not damage windows on hidden workspaces, and visible damage
    // lands at real screen positions, not grid cells. So every overview frame
    // redraws the whole monitor, and the next frame is only requested while
    // something moves; surface commits and an idle timer in HTManager request
    // the rest.
    if (has_pending_motion() || dragged.has_value())
        HTCompat::damage_monitor(monitor);
    const Vector2D transformed_size = HTCompat::monitor_transformed_size(monitor);
    if (!HTLogic::isPositiveFinite(transformed_size.x)
        || !HTLogic::isPositiveFinite(transformed_size.y)) {
        warn_invalid_geometry(
            "grid_render_validation",
            std::format("Invalid monitor render size {}x{}", transformed_size.x, transformed_size.y)
        );
        return;
    }
    CBox monitor_box = {{0, 0}, transformed_size};

    CRectPassElement::SRectData data;
    data.color = CHyprColor {0.F, 0.F, 0.F, 1.F};
    data.box = monitor_box;
    HTCompat::add_rect_pass(data);

    // Do a dance with active workspaces: Hyprland will only properly render the
    // current active one so make the workspace active before rendering it, etc
    if (start_workspace == nullptr)
        return;

    HTScopedWorkspaceVisibility hide_start_workspace(start_workspace, false);

    build_overview_layout(HT_VIEW_ANIMATING);

    // Border plus workspace contents for one grid cell.
    const bool sideways = HTCompat::monitor_transform(monitor) % 2 == 1;
    const auto render_cell = [&](WORKSPACEID ws_id, const CBox& box, PHLWORKSPACE workspace) {
        // renderModif translation used by renderWorkspace is weird so need
        // to scale the translation up as well. Geometry is also calculated from pixel size and not transformed size??
        CBox render_box = {{box.pos() / render_scale}, box.size()};
        if (sideways)
            std::swap(render_box.w, render_box.h);

        CBorderPassElement::SBorderData border;
        border.box = box;
        border.grad1 = selected_workspace_id == ws_id ? active_col : inactive_col;
        border.borderSize = BORDERSIZE;
        HTCompat::add_border_pass(border);

        // Hyprland renders wallpaper and layers for a null workspace as well.
        // Empty grid slots may not have a workspace until a window is moved there.
        HTScopedWorkspaceRender render_workspace(monitor, workspace);
        HTCompat::render_workspace_original(monitor, workspace, time, render_box);
        rendered_workspace_cells = true;
    };

    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    CBox global_mon_box = {monitor_pos, transformed_size};
    for (const auto& [ws_id, ws_layout] : overview_layout) {
        // Skip if the box is empty
        if (ws_layout.box.width < 0.01 || ws_layout.box.height < 0.01)
            continue;

        // render active one last
        const PHLWORKSPACE workspace = HTCompat::workspace_by_id(ws_id);
        if (workspace == start_workspace)
            continue;

        CBox global_box = {ws_layout.box.pos() + monitor_pos, ws_layout.box.size()};
        if (global_box.expand(BORDERSIZE).intersection(global_mon_box).empty())
            continue;

        render_cell(ws_id, ws_layout.box, workspace);
    }

    hide_start_workspace.dismiss();
    HTCompat::set_workspace_render_visibility(start_workspace, true);

    // Render active workspace last so the dragging window is always on top when let go of
    const WORKSPACEID start_id = HTCompat::workspace_id(start_workspace);
    if (const auto* start_layout = find_layout_workspace(start_id); start_layout != nullptr
        && start_layout->box.width > 0.01 && start_layout->box.height > 0.01)
        render_cell(start_id, start_layout->box, start_workspace);

    if (dragged.has_value())
        render_window_at_box(dragged->window, monitor, time, dragged->box);
}

void HTLayoutGrid::request_redraw() {
    const PHLMONITOR monitor = get_monitor();
    if (monitor == nullptr)
        return;

    HTCompat::redraw_monitor(monitor);
}

// Overview or window geometry still animating, so the next frame differs.
bool HTLayoutGrid::has_pending_motion() const {
    if (scale->isBeingAnimated() || offset->isBeingAnimated())
        return true;

    for (const PHLWINDOW& window : HTCompat::compositor_windows()) {
        if (find_layout_workspace(HTCompat::window_workspace_id(window)) != nullptr
            && HTCompat::window_geometry_animating(window))
            return true;
    }
    return false;
}

void HTLayoutGrid::cancel_animation_callbacks() {
    close_render_workspace_id = WORKSPACE_INVALID;
    scale->resetAllCallbacks();
    offset->resetAllCallbacks();
}

PHLMONITOR HTLayoutGrid::get_monitor() {
    const auto par_view = ht_manager->get_view_from_id(view_id);
    if (par_view == nullptr)
        return nullptr;
    return par_view->get_monitor();
}

const HTLayoutGrid::HTWorkspace*
HTLayoutGrid::find_layout_workspace(WORKSPACEID workspace_id) const {
    const auto it = overview_layout.find(workspace_id);
    if (it == overview_layout.end())
        return nullptr;

    return &it->second;
}

WORKSPACEID HTLayoutGrid::get_ws_id_from_global(Vector2D pos) {
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

// The cell itself in global layout coordinates: cell boxes are in monitor
// pixels relative to the monitor, global coordinates are logical.
CBox HTLayoutGrid::get_global_ws_box(WORKSPACEID workspace_id) {
    const PHLMONITOR monitor = get_monitor();
    const float monitor_scale = HTCompat::monitor_scale(monitor);
    const auto* layout_workspace = find_layout_workspace(workspace_id);
    if (monitor == nullptr || layout_workspace == nullptr
        || !HTLogic::isPositiveFinite(monitor_scale))
        return {};

    const CBox& box = layout_workspace->box;
    return {
        box.pos() / monitor_scale + HTCompat::monitor_position(monitor),
        box.size() / monitor_scale
    };
}

// Where workspace_id is drawn right now; nullopt without a usable cell.
std::optional<HTLogic::CellMapping> HTLayoutGrid::cell_mapping(WORKSPACEID workspace_id) {
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
    const double monitor_width = HTCompat::monitor_transformed_size(monitor).x;
    if (!HTLogic::isPositiveFinite(box.w) || !HTLogic::isPositiveFinite(monitor_width))
        return std::nullopt;

    const Vector2D monitor_pos = HTCompat::monitor_position(monitor);
    return HTLogic::CellMapping {
        .cell = {.x = box.x, .y = box.y},
        .width_scale = box.w / monitor_width,
        .monitor_scale = monitor_scale,
        .monitor = {.x = monitor_pos.x, .y = monitor_pos.y},
    };
}

// nullopt without a usable cell, so callers never act on a made-up origin.
std::optional<Vector2D>
HTLayoutGrid::global_to_local_ws_unscaled(Vector2D pos, WORKSPACEID workspace_id) {
    const auto mapping = cell_mapping(workspace_id);
    if (!mapping.has_value())
        return std::nullopt;

    const auto local = HTLogic::globalToWorkspace(*mapping, {.x = pos.x, .y = pos.y});
    if (!HTLogic::isFinitePoint(local.x, local.y))
        return std::nullopt;
    return Vector2D {local.x, local.y};
}

std::optional<Vector2D>
HTLayoutGrid::global_to_workspace_monitor_coords(Vector2D pos, WORKSPACEID workspace_id) {
    const auto local_coords = global_to_local_ws_unscaled(pos, workspace_id);
    if (!local_coords.has_value())
        return std::nullopt;

    return *local_coords + HTCompat::monitor_position(get_monitor());
}

Vector2D HTLayoutGrid::local_ws_unscaled_to_global(Vector2D pos, WORKSPACEID workspace_id) {
    const auto mapping = cell_mapping(workspace_id);
    if (!mapping.has_value())
        return {};

    const auto global = HTLogic::workspaceToGlobal(*mapping, {.x = pos.x, .y = pos.y});
    if (!HTLogic::isFinitePoint(global.x, global.y))
        return {};
    return {global.x, global.y};
}

WORKSPACEID HTLayoutGrid::get_ws_id_from_xy(int x, int y) {
    if (!layout_inputs.has_value())
        return WORKSPACE_INVALID;

    const LayoutShape& shape = layout_inputs->shape;
    if (x < 0 || y < 0 || x >= shape.cols || y >= shape.rows)
        return WORKSPACE_INVALID;

    const WORKSPACEID ws_id = HTLogic::gridWorkspaceID(shape.block, shape.rows, shape.cols, x, y);
    return overview_layout.contains(ws_id) ? ws_id : WORKSPACE_INVALID;
}
