#pragma once

// Pure logic shared by the plugin and the tests; no Hyprland dependency.

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace HTLogic {

using WorkspaceID = long;
using MonitorID = long;

// Compatibility gate

enum class CompatStatus {
    Supported,
    UnsupportedPackageVersion,
    UnsupportedRuntimeVersion,
    RuntimePackageMismatch,
    HeaderRuntimeMismatch,
};

struct CompatDecision {
    bool supported = false;
    CompatStatus status = CompatStatus::Supported;
    std::string error;
};

std::string normalizeVersion(std::string_view version);
bool versionMatchesSupportedTarget(std::string_view version, std::string_view supported_version);
bool versionIsSupported(
    std::string_view version,
    std::span<const std::string_view> supported_versions
);
std::string supportTargetForVersion(
    std::string_view version,
    std::span<const std::string_view> supported_versions
);
std::string supportedVersionsList(std::span<const std::string_view> supported_versions);
CompatDecision decideCompatSupport(
    bool hashes_match,
    std::string_view runtime_version,
    std::string_view package_version,
    std::span<const std::string_view> supported_versions
);

// Geometry

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

// Gestures

enum class OpenSwipeStartAction {
    None,
    ShowOverview,
    HideOverview,
};

OpenSwipeStartAction
resolveOpenSwipeStart(bool vertical, bool active, bool navigating, float delta_y);
bool shouldStartMoveSwipe(bool active, bool navigating);
// Overview open progress in [0, 0.99] for an open_distance > 0.
float openSwipeProgress(float swipe_amount, float open_distance);
bool shouldKeepOverviewOpen(float swipe_amount, float open_distance);

// Mouse interaction

enum class MouseButtonResult {
    Ignore,
    Consume,
};

// Mouse interactions need an open overview that is not closing under the
// cursor, and something to act on.
bool canStartInteraction(bool has_view, bool view_active, bool view_closing, bool has_target);

// Navigation

enum class NavigateArgKind {
    Direction,
    Workspace,
    Invalid,
};

struct NavigateArg {
    NavigateArgKind kind = NavigateArgKind::Invalid;
    std::string direction;
    WorkspaceID workspace_id = -1;
};

NavigateArg parseNavigateArg(std::string_view arg);

struct GridPosition {
    int x = 0;
    int y = 0;
};

bool isGridDirection(std::string_view direction);

// Moves one cell up/down/left/right; wraps when loop is set, otherwise
// returns nullopt when leaving the grid or for an unknown direction.
std::optional<GridPosition>
moveGridInDirection(int x, int y, std::string_view direction, int rows, int cols, bool loop);

// Workspace id for cell (x, y) in a monitor's block of rows * cols workspaces.
WorkspaceID gridWorkspaceID(int block, int rows, int cols, int x, int y);

// Selection state

struct DropWorkspaceDecision {
    bool valid = false;
    WorkspaceID workspace_id = -1;
    bool create_if_missing = false;
    bool snap_to_workspace = false;
};

WorkspaceID resolveSelectionWorkspace(
    WorkspaceID selected_workspace_id,
    WorkspaceID hovered_workspace_id,
    WorkspaceID active_workspace_id
);
WorkspaceID resolveKeyboardSelectionWorkspace(
    WorkspaceID keyboard_workspace_id,
    WorkspaceID active_workspace_id
);
WorkspaceID resolveVisualWorkspaceID(
    WorkspaceID hovered_workspace_id,
    WorkspaceID keyboard_workspace_id,
    WorkspaceID fallback_workspace_id
);
DropWorkspaceDecision resolveDropWorkspace(
    WorkspaceID hovered_workspace_id,
    std::optional<WorkspaceID> dragged_workspace_id
);

// Monitor views

// Each slot is one workspace block, owned by a monitor name. Connected monitors
// keep their slot; new ones take the lowest slot not held by a connected monitor.
std::vector<std::string> assignMonitorBlocks(
    std::vector<std::string> blocks,
    const std::vector<std::string>& connected_names
);
std::optional<int>
monitorBlockIndex(const std::vector<std::string>& blocks, const std::string& monitor_name);

std::vector<MonitorID> missingMonitorViewIDs(
    const std::vector<MonitorID>& view_ids,
    const std::vector<MonitorID>& monitor_ids
);
std::vector<MonitorID> staleMonitorViewIDs(
    const std::vector<MonitorID>& view_ids,
    const std::vector<MonitorID>& monitor_ids
);

} // namespace HTLogic
