#include "geometry_model.hpp"

#include <algorithm>
#include <cmath>

namespace HTLogic {

bool isPositiveFinite(float value) {
    return std::isfinite(value) && value > 0.f;
}

bool isFinitePoint(float x, float y) {
    return std::isfinite(x) && std::isfinite(y);
}

std::optional<int> gridCellCount(int rows, int cols, int max_rows, int max_cols) {
    if (rows <= 0 || cols <= 0 || max_rows <= 0 || max_cols <= 0)
        return std::nullopt;
    if (rows > max_rows || cols > max_cols)
        return std::nullopt;

    return rows * cols;
}

std::optional<GridFit> fitGrid(double width, double height, int rows, int cols, double gap) {
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 || rows <= 0
        || cols <= 0 || !(gap >= 0) || gap > std::min(width, height))
        return std::nullopt;

    double cell_w = (width - gap * (cols + 1)) / cols;
    double cell_h = (height - gap * (rows + 1)) / rows;
    if (cell_w <= 0 || cell_h <= 0)
        return std::nullopt;

    // Shrink one side so cells keep the monitor aspect ratio, and centre the grid.
    GridFit fit {.gap = gap};
    const double aspect = width / height;
    if (cell_h * aspect > cell_w) {
        fit.offset.y = (cell_h - cell_w / aspect) * rows / 2.0;
        cell_h = cell_w / aspect;
    } else if (cell_w / aspect > cell_h) {
        fit.offset.x = (cell_w - cell_h * aspect) * cols / 2.0;
        cell_w = cell_h * aspect;
    }
    fit.opened_scale = cell_w / width;
    return fit;
}

Box gridCellBox(const GridFit& fit, Point monitor_size, int x, int y, double scale, Point offset) {
    const double w = monitor_size.x * scale;
    const double h = monitor_size.y * scale;
    return {
        .x = x * (w + fit.gap) + fit.gap + offset.x + fit.offset.x,
        .y = y * (h + fit.gap) + fit.gap + offset.y + fit.offset.y,
        .w = w,
        .h = h,
    };
}

Point workspaceToGlobal(const CellMapping& m, Point local) {
    return {
        .x = (local.x * m.width_scale * m.monitor_scale + m.cell.x) / m.monitor_scale + m.monitor.x,
        .y = (local.y * m.width_scale * m.monitor_scale + m.cell.y) / m.monitor_scale + m.monitor.y,
    };
}

Point globalToWorkspace(const CellMapping& m, Point global) {
    return {
        .x = ((global.x - m.monitor.x) * m.monitor_scale - m.cell.x) / m.monitor_scale
            / m.width_scale,
        .y = ((global.y - m.monitor.y) * m.monitor_scale - m.cell.y) / m.monitor_scale
            / m.width_scale,
    };
}

std::optional<float> workspaceWidthScale(float workspace_width, float monitor_width) {
    if (!isPositiveFinite(workspace_width) || !isPositiveFinite(monitor_width))
        return std::nullopt;

    return workspace_width / monitor_width;
}

std::optional<float> dragWindowScale(float drag_scale) {
    if (!isPositiveFinite(drag_scale))
        return std::nullopt;

    return drag_scale;
}

std::optional<float> inverseDragWindowScale(float drag_scale) {
    const auto scale = dragWindowScale(drag_scale);
    if (!scale.has_value())
        return std::nullopt;

    return 1.f / *scale;
}

std::optional<float>
windowRenderScale(float box_width, float box_height, float window_width, float window_height) {
    if (!isPositiveFinite(box_width) || !isPositiveFinite(box_height)
        || !isPositiveFinite(window_width) || !isPositiveFinite(window_height))
        return std::nullopt;

    const float scale = box_width / window_width;
    if (!isPositiveFinite(scale))
        return std::nullopt;

    return scale;
}

} // namespace HTLogic
