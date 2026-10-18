#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

#include "../src/logic/compat_model.hpp"
#include "../src/logic/controller_state.hpp"
#include "../src/logic/geometry_model.hpp"
#include "../src/logic/gesture_model.hpp"
#include "../src/logic/interaction_model.hpp"
#include "../src/logic/navigation_model.hpp"
#include "../src/logic/view_sync_model.hpp"

namespace {

void expect(bool condition, std::string_view message) {
    if (condition)
        return;

    std::cerr << "test failure: " << message << '\n';
    std::exit(1);
}

} // namespace

int main() {
    using namespace HTLogic;
    constexpr std::array<std::string_view, 3> SUPPORTED_VERSIONS = {
        "0.55",
        "0.56.0",
        "0.56.2",
    };

    {
        const auto result = decideCompatSupport(true, "0.56.2", "0.56.2", SUPPORTED_VERSIONS);
        expect(result.supported, "compat support should accept supported build versions");
        expect(
            result.status == CompatStatus::Supported,
            "compat support should set supported status"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.55.0", "0.55.0", SUPPORTED_VERSIONS);
        expect(result.supported, "compat support should accept Hyprland 0.55.0");
        expect(
            result.status == CompatStatus::Supported,
            "compat support should set supported status for 0.55.0"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.55.2", "0.55.0", SUPPORTED_VERSIONS);
        expect(result.supported, "compat support should accept supported 0.55 patch drift");
        expect(
            result.status == CompatStatus::Supported,
            "compat support should keep 0.55 patch drift in the supported family"
        );
    }
    {
        const auto result = decideCompatSupport(false, "0.55.0", "0.55.0", SUPPORTED_VERSIONS);
        expect(!result.supported, "compat support should reject mismatched hashes");
        expect(
            result.status == CompatStatus::HeaderRuntimeMismatch,
            "compat support should expose hash mismatch status"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.56.0", "0.56.0", SUPPORTED_VERSIONS);
        expect(result.supported, "compat support should accept Hyprland 0.56.0");
        expect(
            result.status == CompatStatus::Supported,
            "compat support should set supported status for Hyprland 0.56.0"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.55.0", "0.56.0", SUPPORTED_VERSIONS);
        expect(!result.supported, "compat support should reject mismatched package minors");
        expect(
            result.status == CompatStatus::RuntimePackageMismatch,
            "compat support should expose runtime/package mismatch status"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.54.3", "0.54.3", SUPPORTED_VERSIONS);
        expect(!result.supported, "compat support should reject dropped 0.54 releases");
    }
    {
        const auto result = decideCompatSupport(true, "0.55.1", "0.55.1", SUPPORTED_VERSIONS);
        expect(result.supported, "compat support should accept supported 0.55 patch releases");
    }
    expect(
        versionIsSupported("v0.55.2", SUPPORTED_VERSIONS),
        "compat version matching should accept v-prefixed supported minor versions"
    );
    expect(
        !versionIsSupported("0.54.2", SUPPORTED_VERSIONS),
        "compat version matching should reject unsupported patch releases"
    );

    {
        const auto parsed = parseNavigateArg("left");
        expect(
            parsed.kind == NavigateArgKind::Direction && parsed.direction == "left",
            "navigate parser should accept directional args"
        );
    }
    {
        const auto parsed = parseNavigateArg(" 12 ");
        expect(
            parsed.kind == NavigateArgKind::Workspace && parsed.workspace_id == 12,
            "navigate parser should accept positive workspace args"
        );
    }
    {
        const auto parsed = parseNavigateArg("0");
        expect(
            parsed.kind == NavigateArgKind::Invalid,
            "navigate parser should reject non-positive numeric args"
        );
    }
    {
        const auto parsed = parseNavigateArg("spin");
        expect(
            parsed.kind == NavigateArgKind::Invalid,
            "navigate parser should reject unknown textual args"
        );
    }
    {
        const auto result = moveGridInDirection(0, 0, "right", 2, 3, false);
        expect(result.has_value() && result->x == 1 && result->y == 0, "grid move should advance");
    }
    {
        const auto result = moveGridInDirection(0, 0, "left", 2, 3, true);
        expect(result.has_value() && result->x == 2 && result->y == 0, "grid move should loop");
    }
    {
        const auto result = moveGridInDirection(0, 0, "left", 2, 3, false);
        expect(!result.has_value(), "grid move should reject out-of-bounds when loop is disabled");
    }
    {
        const auto result = moveGridInDirection(0, 0, "spin", 2, 3, true);
        expect(!result.has_value(), "grid move should reject invalid directions");
    }
    {
        const auto result = moveGridInDirection(0, 0, "up", 0, 3, true);
        expect(!result.has_value(), "grid move should reject invalid dimensions");
    }

    expect(
        isGridDirection("left") && !isGridDirection("spin"),
        "only grid directions should be valid"
    );
    expect(gridWorkspaceID(0, 2, 3, 0, 0) == 1, "the first block should start at workspace 1");
    expect(gridWorkspaceID(0, 2, 3, 2, 1) == 6, "the first block should end at rows * cols");
    expect(gridWorkspaceID(1, 2, 3, 0, 0) == 7, "the second block should follow the first");
    expect(gridWorkspaceID(1, 2, 3, 2, 1) == 12, "the second block should end at 2 * rows * cols");
    {
        const auto result = moveGridInDirection(1, 1, "up", 2, 3, true);
        expect(result.has_value() && result->x == 1 && result->y == 0, "grid move should go up");
    }
    {
        const auto result = moveGridInDirection(2, 1, "down", 2, 3, true);
        expect(
            result.has_value() && result->x == 2 && result->y == 0,
            "grid move should wrap down"
        );
    }

    expect(HTLogic::isPositiveFinite(1.0f), "positive finite values should be accepted");
    expect(!HTLogic::isPositiveFinite(0.0f), "zero should be rejected as invalid geometry");
    expect(
        !HTLogic::isPositiveFinite(-1.0f),
        "negative values should be rejected as invalid geometry"
    );
    expect(HTLogic::isFinitePoint(1.0f, 2.0f), "finite points should be accepted");
    expect(!HTLogic::isFinitePoint(INFINITY, 2.0f), "non-finite x should be rejected");
    expect(!HTLogic::isFinitePoint(1.0f, -INFINITY), "non-finite y should be rejected");
    {
        const auto result = HTLogic::workspaceWidthScale(200.0f, 100.0f);
        expect(
            result.has_value() && *result == 2.0f,
            "workspace width scale should divide valid widths"
        );
    }
    {
        const auto result = HTLogic::gridCellCount(3, 4, 8, 8);
        expect(result.has_value() && *result == 12, "grid cell count should accept bounded grids");
    }
    {
        const auto result = HTLogic::gridCellCount(9, 4, 8, 8);
        expect(!result.has_value(), "grid cell count should reject too many rows");
    }
    {
        const auto result = HTLogic::gridCellCount(3, 0, 8, 8);
        expect(!result.has_value(), "grid cell count should reject non-positive columns");
    }
    {
        const auto result = HTLogic::workspaceWidthScale(200.0f, 0.0f);
        expect(!result.has_value(), "workspace width scale should reject zero monitor width");
    }
    {
        const auto result = HTLogic::dragWindowScale(1.5f);
        expect(
            result.has_value() && *result == 1.5f,
            "drag window scale should accept positive finite values"
        );
    }
    {
        const auto result = HTLogic::dragWindowScale(0.0f);
        expect(!result.has_value(), "drag window scale should reject zero");
    }
    {
        const auto result = HTLogic::inverseDragWindowScale(2.0f);
        expect(
            result.has_value() && *result == 0.5f,
            "inverse drag window scale should invert valid scales"
        );
    }
    {
        const auto result = HTLogic::inverseDragWindowScale(-1.0f);
        expect(!result.has_value(), "inverse drag window scale should reject invalid scales");
    }
    {
        const auto result = HTLogic::windowRenderScale(150.0f, 100.0f, 75.0f, 50.0f);
        expect(
            result.has_value() && *result == 2.0f,
            "window render scale should divide valid widths"
        );
    }
    {
        const auto result = HTLogic::windowRenderScale(150.0f, 100.0f, 0.0f, 50.0f);
        expect(!result.has_value(), "window render scale should reject zero window width");
    }

    expect(
        resolveSelectionWorkspace(8, 9, 3) == 8,
        "selection should prefer the keyboard-selected workspace"
    );
    expect(
        resolveSelectionWorkspace(-1, 9, 3) == 9,
        "selection should fall back to hovered workspace when no keyboard selection exists"
    );
    expect(
        resolveSelectionWorkspace(-1, -1, 3) == 3,
        "selection should fall back to the active workspace"
    );
    expect(
        resolveKeyboardSelectionWorkspace(8, 3) == 8,
        "keyboard selection should prefer remembered keyboard workspaces"
    );
    expect(
        resolveKeyboardSelectionWorkspace(-1, 3) == 3,
        "keyboard selection should fall back to the active workspace"
    );
    expect(
        resolveVisualWorkspaceID(true, 9, 3, 1) == 9,
        "visual selection should prefer hovered workspace when hover is active"
    );
    expect(
        resolveVisualWorkspaceID(false, 9, 3, 1) == 3,
        "visual selection should prefer keyboard selection when hover is inactive"
    );
    expect(
        resolveVisualWorkspaceID(false, -1, -1, 1) == 1,
        "visual selection should fall back when no hover or keyboard selection exists"
    );

    {
        const auto result = resolveDropWorkspace(12, std::optional<long> {7});
        expect(result.valid, "drop resolution should accept hovered workspaces");
        expect(result.workspace_id == 12, "drop resolution should use the hovered workspace id");
        expect(result.create_if_missing, "hovered workspace drops should allow creation");
        expect(!result.snap_to_workspace, "hovered workspace drops should not snap");
    }
    {
        const auto result = resolveDropWorkspace(-1, std::optional<long> {7});
        expect(result.valid, "drop resolution should fall back to the dragged workspace");
        expect(result.workspace_id == 7, "drop resolution should use the dragged workspace id");
        expect(
            !result.create_if_missing,
            "fallback workspace drops should not create new workspaces"
        );
        expect(result.snap_to_workspace, "fallback workspace drops should snap to the workspace");
    }
    {
        const auto result = resolveDropWorkspace(-1, std::nullopt);
        expect(
            !result.valid,
            "drop resolution should reject missing hovered and dragged workspaces"
        );
    }
    {
        const auto result = resolveDropWorkspace(-1, std::optional<long> {42});
        expect(
            result.valid,
            "drop resolution should accept invalid-hover fallback with dragged workspace"
        );
        expect(
            result.workspace_id == 42,
            "drop resolution fallback should keep the dragged workspace id"
        );
        expect(
            result.snap_to_workspace,
            "drop resolution fallback should snap to the dragged workspace"
        );
    }

    expect(
        canStartInteraction(true, true, false, true),
        "interactions should start on an open overview with a target"
    );
    expect(
        !canStartInteraction(true, true, false, false),
        "interactions should not start without a target"
    );
    expect(
        !canStartInteraction(true, false, false, true),
        "interactions should not start on a closed overview"
    );
    expect(
        !canStartInteraction(true, true, true, true),
        "interactions should not start while the overview closes"
    );

    {
        const auto blocks = assignMonitorBlocks({}, {"eDP-1", "DP-1"});
        expect(
            monitorBlockIndex(blocks, "eDP-1") == 0 && monitorBlockIndex(blocks, "DP-1") == 1,
            "first-seen monitors should take blocks in connection order"
        );
        const auto edp_off = assignMonitorBlocks(blocks, {"DP-1"});
        expect(
            monitorBlockIndex(edp_off, "DP-1") == 1,
            "a monitor should keep its block when a lower monitor disconnects"
        );
        const auto edp_back = assignMonitorBlocks(edp_off, {"DP-1", "eDP-1"});
        expect(
            monitorBlockIndex(edp_back, "eDP-1") == 0 && monitorBlockIndex(edp_back, "DP-1") == 1,
            "a reconnected monitor should regain its block"
        );
        const auto replaced = assignMonitorBlocks(edp_off, {"DP-1", "HDMI-A-1"});
        expect(
            monitorBlockIndex(replaced, "HDMI-A-1") == 0,
            "a new monitor should reuse the lowest block of a disconnected monitor"
        );
        expect(
            !monitorBlockIndex(replaced, "eDP-1").has_value(),
            "a replaced monitor should not own a workspace block"
        );
    }
    {
        const auto blocks = assignMonitorBlocks({"DP-1"}, {"DP-2"});
        expect(
            monitorBlockIndex(blocks, "DP-2") == 0,
            "a reconnected sole monitor should take the first workspace block"
        );
    }

    {
        const auto result = missingMonitorViewIDs({1, 3}, {1, 2, 3, 4});
        expect(
            result == std::vector<long>({2, 4}),
            "missing monitor ids should preserve monitor order"
        );
    }
    {
        const auto result = staleMonitorViewIDs({1, 2, 5}, {2, 3, 4});
        expect(result == std::vector<long>({1, 5}), "stale monitor ids should preserve view order");
    }

    expect(
        resolveOpenSwipeStart(true, false, false, -1.0f) == OpenSwipeStartAction::ShowOverview,
        "vertical negative swipes should open inactive overviews"
    );
    expect(
        resolveOpenSwipeStart(true, true, false, 1.0f) == OpenSwipeStartAction::HideOverview,
        "vertical positive swipes should close active overviews"
    );
    expect(
        resolveOpenSwipeStart(false, false, false, -1.0f) == OpenSwipeStartAction::None,
        "horizontal swipes should not trigger open gestures"
    );
    expect(
        resolveOpenSwipeStart(true, false, true, -1.0f) == OpenSwipeStartAction::None,
        "navigating views should reject new open gestures"
    );
    expect(shouldStartMoveSwipe(false, false), "inactive stable views should allow move swipes");
    expect(!shouldStartMoveSwipe(true, false), "active views should reject move swipes");
    expect(!shouldStartMoveSwipe(false, true), "navigating views should reject move swipes");

    expect(
        openSwipeProgress(50.0f, 100.0f) == 0.5f,
        "open swipe progress should map distance to progress"
    );
    expect(
        shouldKeepOverviewOpen(20.0f, 100.0f),
        "low swipe progress should keep the overview open"
    );
    expect(!shouldKeepOverviewOpen(80.0f, 100.0f), "high swipe progress should close the overview");

    // Grid fit: every opened cell lies on the monitor, keeps its aspect ratio
    // and the grid is centred, for several grid sizes and monitor shapes.
    {
        const std::array<std::array<double, 2>, 3> monitors = {
            {{2560, 1440}, {1080, 1920}, {1333, 777}}
        };
        const std::array<std::array<int, 2>, 4> grids = {{{1, 1}, {3, 3}, {2, 5}, {8, 8}}};
        for (const auto& [width, height] : monitors) {
            for (const auto& [rows, cols] : grids) {
                const auto fit = fitGrid(width, height, rows, cols, 8.0);
                expect(fit.has_value(), "grid fit should succeed for normal monitors");
                const auto first = gridCellBox(*fit, {width, height}, 0, 0, fit->opened_scale, {});
                const auto last =
                    gridCellBox(*fit, {width, height}, cols - 1, rows - 1, fit->opened_scale, {});
                expect(first.x >= 0 && first.y >= 0, "opened grid should start on the monitor");
                expect(
                    last.x + last.w <= width + 1e-6 && last.y + last.h <= height + 1e-6,
                    "opened grid should end on the monitor"
                );
                expect(
                    std::abs(first.w / first.h - width / height) < 1e-9,
                    "opened cells should keep the monitor aspect ratio"
                );
                expect(
                    std::abs(first.x - (width - (last.x + last.w))) < 1e-6
                        && std::abs(first.y - (height - (last.y + last.h))) < 1e-6,
                    "opened grid should be centred"
                );
            }
        }
    }
    expect(!fitGrid(100, 100, 3, 3, 101).has_value(), "gaps larger than the monitor should fail");
    expect(!fitGrid(100, 100, 8, 8, 12).has_value(), "gaps leaving no room for cells should fail");
    expect(!fitGrid(0, 100, 3, 3, 8).has_value(), "zero-size monitors should fail");
    expect(!fitGrid(NAN, 100, 3, 3, 8).has_value(), "non-finite monitor sizes should fail");
    expect(!fitGrid(100, 100, 3, 3, NAN).has_value(), "non-finite gaps should fail");
    {
        const auto fit = fitGrid(1920, 1080, 3, 3, 8.0);
        const auto closed = gridCellBox(*fit, {1920, 1080}, 1, 2, 1.0, {-100, -200});
        expect(closed.w == 1920 && closed.h == 1080, "closed cells should be monitor-sized");
        expect(
            std::abs(closed.x - (1928 + 8 - 100 + fit->offset.x)) < 1e-9
                && std::abs(closed.y - (2 * 1088 + 8 - 200 + fit->offset.y)) < 1e-9,
            "cells should step by size plus gap and follow the grid offset"
        );
    }

    // Workspace <-> global coordinates round-trip, and the cell origin maps to
    // the workspace origin.
    {
        const CellMapping mapping {
            .cell = {.x = 640, .y = 360},
            .width_scale = 0.3,
            .monitor_scale = 1.5,
            .monitor = {.x = 2560, .y = 0},
        };
        for (const Point local : {Point {0, 0}, Point {123.5, 456.25}, Point {-10, 2000}}) {
            const Point back = globalToWorkspace(mapping, workspaceToGlobal(mapping, local));
            expect(
                std::abs(back.x - local.x) < 1e-9 && std::abs(back.y - local.y) < 1e-9,
                "workspace/global coordinates should round-trip"
            );
        }
        const Point origin = workspaceToGlobal(mapping, {0, 0});
        expect(
            std::abs(origin.x - (2560 + 640 / 1.5)) < 1e-9 && std::abs(origin.y - 360 / 1.5) < 1e-9,
            "the workspace origin should map to the cell origin in logical pixels"
        );
    }

    // Grid navigation over every cell and size: edges stop without loop and
    // wrap with it.
    for (int rows = 1; rows <= 8; rows++) {
        for (int cols = 1; cols <= 8; cols++) {
            for (int y = 0; y < rows; y++) {
                for (int x = 0; x < cols; x++) {
                    const auto right = moveGridInDirection(x, y, "right", rows, cols, false);
                    expect(
                        right.has_value() == (x + 1 < cols),
                        "moving right should stop only at the last column"
                    );
                    const auto down = moveGridInDirection(x, y, "down", rows, cols, true);
                    expect(
                        down.has_value() && down->x == x && down->y == (y + 1) % rows,
                        "moving down with loop should wrap to the first row"
                    );
                    const auto left = moveGridInDirection(x, y, "left", rows, cols, true);
                    expect(
                        left.has_value() && left->x == (x + cols - 1) % cols && left->y == y,
                        "moving left with loop should wrap to the last column"
                    );
                }
            }
        }
    }

    expect(parseNavigateArg(" up ").kind == NavigateArgKind::Direction, "directions may be padded");
    for (const std::string_view bad : {"", "  ", "-3", "3x", "0", "99999999999999999999", "upp"})
        expect(
            parseNavigateArg(bad).kind == NavigateArgKind::Invalid,
            "bad navigate args should be invalid"
        );
    {
        const auto arg = parseNavigateArg("+3");
        expect(arg.kind == NavigateArgKind::Invalid, "signed workspace numbers should be invalid");
    }

    return 0;
}
