#pragma once

#include <optional>

namespace HTLogic {

bool isPositiveFinite(float value);
bool isFinitePoint(float x, float y);
std::optional<int> gridCellCount(int rows, int cols, int max_rows, int max_cols);
std::optional<float> workspaceWidthScale(float workspace_width, float monitor_width);
std::optional<float> dragWindowScale(float drag_scale);
std::optional<float> inverseDragWindowScale(float drag_scale);
std::optional<float>
windowRenderScale(float box_width, float box_height, float window_width, float window_height);

struct Point {
    double x = 0;
    double y = 0;
};

struct Box {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
};

// How rows x cols monitor-shaped cells, with gap pixels between and around
// them, fit centred on a width x height monitor.
struct GridFit {
    double gap = 0;
    Point offset;
    // Cell width / monitor width with the overview fully open.
    double opened_scale = 1;
};

// nullopt when the monitor size is invalid or the gaps leave no room for cells.
std::optional<GridFit> fitGrid(double width, double height, int rows, int cols, double gap);

// Cell (x, y) of a fitted grid whose cells are scale times the monitor size,
// with the whole grid shifted by offset.
Box gridCellBox(const GridFit& fit, Point monitor_size, int x, int y, double scale, Point offset);

// A workspace drawn in a grid cell: the cell origin in monitor pixels, cell
// width / monitor width, the monitor scale and its logical position.
struct CellMapping {
    Point cell;
    double width_scale = 1;
    double monitor_scale = 1;
    Point monitor;
};

// Logical workspace coordinates (relative to the monitor) to global layout
// coordinates on screen, and back.
Point workspaceToGlobal(const CellMapping& mapping, Point local);
Point globalToWorkspace(const CellMapping& mapping, Point global);

} // namespace HTLogic
