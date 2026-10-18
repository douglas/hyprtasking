#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

#include "../src/logic/logic.hpp"

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
    const auto compat_error =
        [&](bool hashes_match, std::string_view runtime, std::string_view package) {
            return compatSupportError(hashes_match, runtime, package, SUPPORTED_VERSIONS);
        };

    expect(!compat_error(true, "0.56.2", "0.56.2"), "compat support should accept 0.56.2");
    expect(!compat_error(true, "0.56.0", "0.56.0"), "compat support should accept 0.56.0");
    expect(!compat_error(true, "0.55.0", "0.55.0"), "compat support should accept 0.55.0");
    expect(
        !compat_error(true, "0.55.2", "0.55.0"),
        "compat support should accept 0.55 patch drift between runtime and package"
    );
    expect(
        !compat_error(true, "v0.55.2", "0.55.0"),
        "compat support should accept v-prefixed runtime versions"
    );
    expect(
        compat_error(false, "0.55.0", "0.55.0")
            == "mismatched Hyprland headers and compositor build for version 0.55.0",
        "compat support should reject mismatched hashes"
    );
    expect(
        compat_error(true, "0.55.0", "0.56.0")
            == "installed Hyprland package 0.56.0 does not match running Hyprland 0.55.0",
        "compat support should reject mismatched package minors"
    );
    expect(
        compat_error(
            true,
            "0.54.3",
            "0.54.3"
        ) == "built against unsupported Hyprland package 0.54.3 (supported: 0.55, 0.56.0, 0.56.2)",
        "compat support should reject dropped 0.54 packages"
    );
    expect(
        compat_error(true, "0.54.2", "0.55.0")
            == "running on unsupported Hyprland runtime 0.54.2 (supported: 0.55, 0.56.0, 0.56.2)",
        "compat support should reject dropped 0.54 runtimes"
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
    expect(parseNavigateArg(" up ").kind == NavigateArgKind::Direction, "directions may be padded");
    for (const std::string_view bad :
         {"", "  ", "-3", "3x", "0", "+3", "99999999999999999999", "upp", "spin"})
        expect(
            parseNavigateArg(bad).kind == NavigateArgKind::Invalid,
            "bad navigate args should be invalid"
        );

    expect(
        isGridDirection("left") && !isGridDirection("spin") && !isGridDirection(" up "),
        "only unpadded grid directions should be valid"
    );
    {
        const auto result = moveGridInDirection(0, 0, "left", 2, 3, false);
        expect(!result.has_value(), "grid move should reject out-of-bounds when loop is disabled");
    }
    {
        const auto result = moveGridInDirection(1, 1, "up", 2, 3, true);
        expect(result.has_value() && result->x == 1 && result->y == 0, "grid move should go up");
    }
    {
        const auto result = moveGridInDirection(0, 0, "spin", 2, 3, true);
        expect(!result.has_value(), "grid move should reject invalid directions");
    }
    {
        const auto result = moveGridInDirection(0, 0, "up", 0, 3, true);
        expect(!result.has_value(), "grid move should reject invalid dimensions");
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

    expect(gridWorkspaceID(0, 2, 3, 0, 0) == 1, "the first block should start at workspace 1");
    expect(gridWorkspaceID(0, 2, 3, 2, 1) == 6, "the first block should end at rows * cols");
    expect(gridWorkspaceID(1, 2, 3, 0, 0) == 7, "the second block should follow the first");
    expect(gridWorkspaceID(1, 2, 3, 2, 1) == 12, "the second block should end at 2 * rows * cols");

    expect(isPositiveFinite(1.0), "positive finite values should be accepted");
    expect(!isPositiveFinite(0.0), "zero should be rejected as invalid geometry");
    expect(!isPositiveFinite(-1.0), "negative values should be rejected as invalid geometry");
    expect(!isPositiveFinite(NAN), "NaN should be rejected as invalid geometry");
    expect(isFinitePoint(1.0, 2.0), "finite points should be accepted");
    expect(!isFinitePoint(INFINITY, 2.0), "non-finite x should be rejected");
    expect(!isFinitePoint(1.0, -INFINITY), "non-finite y should be rejected");
    {
        const auto result = windowRenderScale(150.0f, 75.0f);
        expect(result.has_value() && *result == 2.0f, "window render scale should divide widths");
    }
    expect(
        !windowRenderScale(150.0f, 0.0f).has_value(),
        "window render scale should reject zero window width"
    );

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
        resolveVisualWorkspaceID(9, 3, 1) == 9,
        "visual selection should prefer the hovered workspace"
    );
    expect(
        resolveVisualWorkspaceID(-1, 3, 1) == 3,
        "visual selection should prefer keyboard selection without a hover"
    );
    expect(
        resolveVisualWorkspaceID(-1, -1, 1) == 1,
        "visual selection should fall back when no hover or keyboard selection exists"
    );

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

    expect(
        idsNotIn({1, 2, 3, 4}, {1, 3}) == std::vector<long>({2, 4}),
        "missing ids should keep their order"
    );
    expect(
        idsNotIn({1, 2, 5}, {2, 3, 4}) == std::vector<long>({1, 5}),
        "stale ids should keep their order"
    );

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

    return 0;
}
