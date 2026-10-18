#include "logic.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <format>
#include <iterator>

namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return value;
}

std::string normalizeVersion(std::string_view version) {
    version = trim(version);
    if (version.starts_with('v'))
        version.remove_prefix(1);

    return std::string(version);
}

bool versionMatchesSupportedTarget(std::string_view version, std::string_view supported_version) {
    const auto normalized = normalizeVersion(version);
    const auto supported = normalizeVersion(supported_version);

    return normalized == supported || normalized.starts_with(supported + ".");
}

// The normalized supported version that version belongs to, empty if none.
std::string supportTargetForVersion(
    std::string_view version,
    std::span<const std::string_view> supported_versions
) {
    const auto target =
        std::ranges::find_if(supported_versions, [&](const std::string_view candidate) {
            return versionMatchesSupportedTarget(version, candidate);
        });

    if (target == supported_versions.end())
        return "";

    return normalizeVersion(*target);
}

std::string supportedVersionsList(std::span<const std::string_view> supported_versions) {
    std::string result;

    for (size_t i = 0; i < supported_versions.size(); ++i) {
        if (i > 0)
            result += ", ";

        result += normalizeVersion(supported_versions[i]);
    }

    return result;
}

} // namespace

namespace HTLogic {

std::optional<std::string> compatSupportError(
    bool hashes_match,
    std::string_view runtime_version,
    std::string_view package_version,
    std::span<const std::string_view> supported_versions
) {
    const auto normalized_package = normalizeVersion(package_version);
    const auto normalized_runtime = normalizeVersion(runtime_version);
    const auto package_support_target =
        supportTargetForVersion(package_version, supported_versions);
    const auto runtime_support_target =
        supportTargetForVersion(runtime_version, supported_versions);

    if (package_support_target.empty()) {
        return std::format(
            "built against unsupported Hyprland package {} (supported: {})",
            normalized_package,
            supportedVersionsList(supported_versions)
        );
    }

    if (runtime_support_target.empty()) {
        return std::format(
            "running on unsupported Hyprland runtime {} (supported: {})",
            normalized_runtime,
            supportedVersionsList(supported_versions)
        );
    }

    if (runtime_support_target != package_support_target) {
        return "installed Hyprland package " + normalized_package
            + " does not match running Hyprland " + normalized_runtime;
    }

    if (!hashes_match)
        return "mismatched Hyprland headers and compositor build for version " + normalized_runtime;

    return std::nullopt;
}

bool isPositiveFinite(double value) {
    return std::isfinite(value) && value > 0;
}

bool isFinitePoint(double x, double y) {
    return std::isfinite(x) && std::isfinite(y);
}

std::optional<float> windowRenderScale(float box_width, float window_width) {
    if (!isPositiveFinite(box_width) || !isPositiveFinite(window_width))
        return std::nullopt;

    const float scale = box_width / window_width;
    if (!isPositiveFinite(scale))
        return std::nullopt;

    return scale;
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

OpenSwipeStartAction
resolveOpenSwipeStart(bool vertical, bool active, bool navigating, float delta_y) {
    if (!vertical || navigating)
        return OpenSwipeStartAction::None;

    if (!active && delta_y <= 0.f)
        return OpenSwipeStartAction::ShowOverview;
    if (active && delta_y > 0.f)
        return OpenSwipeStartAction::HideOverview;

    return OpenSwipeStartAction::None;
}

bool shouldStartMoveSwipe(bool active, bool navigating) {
    return !active && !navigating;
}

float openSwipeProgress(float swipe_amount, float open_distance) {
    return 1.0f - std::clamp(swipe_amount / open_distance, 0.01f, 1.0f);
}

bool shouldKeepOverviewOpen(float swipe_amount, float open_distance) {
    return openSwipeProgress(swipe_amount, open_distance) >= 0.5f;
}

bool canStartInteraction(bool has_view, bool view_active, bool view_closing, bool has_target) {
    return has_view && view_active && !view_closing && has_target;
}

NavigateArg parseNavigateArg(std::string_view arg) {
    const std::string_view trimmed = trim(arg);
    if (isGridDirection(trimmed)) {
        return {
            .kind = NavigateArgKind::Direction,
            .direction = std::string(trimmed),
        };
    }

    WorkspaceID workspace_id = -1;
    const auto* begin = trimmed.data();
    const auto* end = trimmed.data() + trimmed.size();
    const auto [ptr, ec] = std::from_chars(begin, end, workspace_id);
    if (ec == std::errc {} && ptr == end && workspace_id > 0) {
        return {
            .kind = NavigateArgKind::Workspace,
            .workspace_id = workspace_id,
        };
    }

    return {.kind = NavigateArgKind::Invalid};
}

bool isGridDirection(std::string_view direction) {
    return direction == "up" || direction == "down" || direction == "left" || direction == "right";
}

std::optional<GridPosition>
moveGridInDirection(int x, int y, std::string_view direction, int rows, int cols, bool loop) {
    if (rows <= 0 || cols <= 0)
        return std::nullopt;

    if (direction == "up")
        y--;
    else if (direction == "down")
        y++;
    else if (direction == "right")
        x++;
    else if (direction == "left")
        x--;
    else
        return std::nullopt;

    if (loop)
        return GridPosition {.x = (x + cols) % cols, .y = (y + rows) % rows};

    if (x < 0 || x >= cols || y < 0 || y >= rows)
        return std::nullopt;

    return GridPosition {.x = x, .y = y};
}

WorkspaceID gridWorkspaceID(int block, int rows, int cols, int x, int y) {
    return static_cast<WorkspaceID>(block * rows + y) * cols + x + 1;
}

WorkspaceID resolveSelectionWorkspace(
    WorkspaceID selected_workspace_id,
    WorkspaceID hovered_workspace_id,
    WorkspaceID active_workspace_id
) {
    if (selected_workspace_id >= 0)
        return selected_workspace_id;
    if (hovered_workspace_id >= 0)
        return hovered_workspace_id;

    return active_workspace_id;
}

WorkspaceID resolveKeyboardSelectionWorkspace(
    WorkspaceID keyboard_workspace_id,
    WorkspaceID active_workspace_id
) {
    if (keyboard_workspace_id >= 0)
        return keyboard_workspace_id;

    return active_workspace_id;
}

WorkspaceID resolveVisualWorkspaceID(
    WorkspaceID hovered_workspace_id,
    WorkspaceID keyboard_workspace_id,
    WorkspaceID fallback_workspace_id
) {
    if (hovered_workspace_id >= 0)
        return hovered_workspace_id;
    if (keyboard_workspace_id >= 0)
        return keyboard_workspace_id;

    return fallback_workspace_id;
}

std::vector<std::string> assignMonitorBlocks(
    std::vector<std::string> blocks,
    const std::vector<std::string>& connected_names
) {
    const auto is_connected = [&](const std::string& name) {
        return std::ranges::find(connected_names, name) != connected_names.end();
    };

    for (const auto& name : connected_names) {
        if (std::ranges::find(blocks, name) != blocks.end())
            continue;

        const auto free_slot = std::ranges::find_if(blocks, [&](const std::string& owner) {
            return !is_connected(owner);
        });
        if (free_slot != blocks.end())
            *free_slot = name;
        else
            blocks.push_back(name);
    }

    return blocks;
}

std::optional<int>
monitorBlockIndex(const std::vector<std::string>& blocks, const std::string& monitor_name) {
    const auto it = std::ranges::find(blocks, monitor_name);
    if (it == blocks.end())
        return std::nullopt;

    return static_cast<int>(std::distance(blocks.begin(), it));
}

std::vector<long> idsNotIn(const std::vector<long>& ids, const std::vector<long>& others) {
    std::vector<long> result;
    for (const long id : ids) {
        if (std::ranges::find(others, id) == others.end())
            result.push_back(id);
    }

    return result;
}

} // namespace HTLogic
