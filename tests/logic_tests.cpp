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

struct GridPosition {
    int x = 0;
    int y = 0;
};

std::optional<GridPosition> moveInDirection(int x, int y, std::string_view direction) {
    if (direction == "up")
        return GridPosition {.x = x, .y = y - 1};
    if (direction == "down")
        return GridPosition {.x = x, .y = y + 1};
    if (direction == "right")
        return GridPosition {.x = x + 1, .y = y};
    if (direction == "left")
        return GridPosition {.x = x - 1, .y = y};

    return std::nullopt;
}

std::optional<GridPosition>
moveGridInDirection(int x, int y, std::string_view direction, int rows, int cols, bool loop) {
    if (rows <= 0 || cols <= 0)
        return std::nullopt;

    const auto moved = moveInDirection(x, y, direction);
    if (!moved.has_value())
        return std::nullopt;

    GridPosition result = *moved;
    if (loop) {
        result.x = (result.x + cols) % cols;
        result.y = (result.y + rows) % rows;
        return result;
    }

    if (result.x < 0 || result.x >= cols || result.y < 0 || result.y >= rows)
        return std::nullopt;

    return result;
}

HTLogic::WorkspaceID
gridWorkspaceID(long view_id, int first_ws_offset, int rows, int cols, int x, int y) {
    const int ws_per_view = rows * cols;
    return view_id * ws_per_view + first_ws_offset + (view_id * rows + y) * cols + x + 1;
}

} // namespace

int main() {
    using namespace HTLogic;
    constexpr std::array<std::string_view, 2> SUPPORTED_VERSIONS = {
        "0.54.3",
        "0.55.0",
    };

    {
        const auto result = decideCompatSupport(true, "0.54.3", "0.54.3", SUPPORTED_VERSIONS);
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
        const auto result = decideCompatSupport(false, "0.55.0", "0.55.0", SUPPORTED_VERSIONS);
        expect(!result.supported, "compat support should reject mismatched hashes");
        expect(
            result.status == CompatStatus::HeaderRuntimeMismatch,
            "compat support should expose hash mismatch status"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.56.0", "0.55.0", SUPPORTED_VERSIONS);
        expect(!result.supported, "compat support should reject unsupported Hyprland runtimes");
        expect(
            result.status == CompatStatus::UnsupportedRuntimeVersion,
            "compat support should expose unsupported runtime status"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.55.0", "0.56.0", SUPPORTED_VERSIONS);
        expect(!result.supported, "compat support should reject unsupported package minors");
        expect(
            result.status == CompatStatus::UnsupportedPackageVersion,
            "compat support should expose unsupported package status"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.55.0", "0.54.3", SUPPORTED_VERSIONS);
        expect(!result.supported, "compat support should reject package and runtime drift");
        expect(
            result.status == CompatStatus::RuntimePackageMismatch,
            "compat support should expose runtime/package mismatch status"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.54.2", "0.54.2", SUPPORTED_VERSIONS);
        expect(
            !result.supported,
            "compat support should reject older unsupported 0.54 patch releases"
        );
    }
    {
        const auto result = decideCompatSupport(true, "0.55.1", "0.55.1", SUPPORTED_VERSIONS);
        expect(!result.supported, "compat support should hard-reject unknown patch releases");
    }
    expect(
        versionIsSupported("v0.55.0", SUPPORTED_VERSIONS),
        "compat version matching should accept v-prefixed supported versions"
    );
    expect(
        !versionIsSupported("0.54.2", SUPPORTED_VERSIONS),
        "compat version matching should reject unsupported patch releases"
    );
    expect(
        versionMatchesExactly("v0.55.0", "0.55.0"),
        "compat version matching should normalize exact versions"
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
            parsed.kind == NavigateArgKind::Invalid,
            "navigate parser should reject numeric slot args"
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
        gridWorkspaceID(0, 0, 2, 3, 2, 1) == 6,
        "grid workspace ids should match the first view"
    );
    expect(gridWorkspaceID(1, 6, 2, 3, 2, 1) == 24, "grid workspace ids should match offset views");

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
        decideDragStart(true, true, false, true) == DragStartAction::BeginDrag,
        "drag start should proceed with a valid workspace target"
    );
    expect(
        decideDragStart(true, true, false, false) == DragStartAction::Ignore,
        "drag start should reject non-workspace targets"
    );

    expect(
        decideDragEnd(true, true, false, true, true, true) == DragEndAction::FinalizeDrop,
        "drag end should finalize valid move drops"
    );
    expect(
        decideDragEnd(true, true, false, true, false, true) == DragEndAction::Ignore,
        "drag end should reject missing dragged windows"
    );
    expect(
        decideDragEnd(true, true, false, true, true, false) == DragEndAction::Ignore,
        "drag end should reject non-move drag modes"
    );
    expect(
        decideSelectStart(true, true, false, true) == SelectStartAction::BeginSelect,
        "select start should begin for valid active workspace targets"
    );
    expect(
        decideSelectStart(true, true, false, false) == SelectStartAction::Ignore,
        "select start should ignore invalid workspace targets"
    );
    expect(
        decideSelectEnd(true, true, true, false, true) == SelectEndAction::FinalizeSelect,
        "select end should finalize valid pending selections"
    );
    expect(
        decideSelectEnd(true, true, true, false, false) == SelectEndAction::CancelSelect,
        "select end should cancel pending selections released off target"
    );
    expect(
        decideSelectEnd(false, true, true, false, true) == SelectEndAction::Ignore,
        "select end should ignore releases without a pending selection"
    );
    expect(
        shouldConsumeManagedMouseButton(true, true, false, true),
        "managed overview buttons should be consumed when active"
    );
    expect(
        !shouldConsumeManagedMouseButton(true, false, false, true),
        "inactive overviews should not consume managed mouse buttons"
    );
    expect(
        !shouldConsumeManagedMouseButton(true, true, false, false),
        "unbound mouse buttons should not be consumed"
    );
    expect(
        resolveClaimedMouseRelease(false, false, false) == MouseButtonResult::Ignore,
        "unclaimed mouse releases should be ignored"
    );
    expect(
        resolveClaimedMouseRelease(true, false, false) == MouseButtonResult::Consume,
        "claimed no-op releases should stay consumed"
    );
    expect(
        resolveClaimedMouseRelease(true, true, false) == MouseButtonResult::Consume,
        "claimed drag releases should stay consumed when no passthrough is requested"
    );
    expect(
        resolveClaimedMouseRelease(true, true, true) == MouseButtonResult::PassThrough,
        "successful drag releases should allow passthrough"
    );

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
        detectSwipeDirection(4.0, 1.0) == SwipeDirection::Horizontal,
        "swipe direction should prefer horizontal deltas"
    );
    expect(
        detectSwipeDirection(1.0, 4.0) == SwipeDirection::Vertical,
        "swipe direction should prefer vertical deltas"
    );
    expect(
        detectSwipeDirection(2.0, 2.0) == SwipeDirection::None,
        "swipe direction should reject equal deltas"
    );

    expect(normalizedOpenDelta(5.0f, true) == 5.0f, "open-positive delta should be unchanged");
    expect(normalizedOpenDelta(5.0f, false) == -5.0f, "open-negative delta should be inverted");
    expect(shouldConsumeOpenSwipe(true, false), "active views should consume open swipes");
    expect(shouldConsumeOpenSwipe(false, true), "existing open swipes should stay consumed");
    expect(
        !shouldConsumeOpenSwipe(false, false),
        "inactive idle views should not consume open swipes"
    );
    expect(shouldConsumeMoveSwipe(true), "existing move swipes should stay consumed");
    expect(!shouldConsumeMoveSwipe(false), "idle move swipes should not be consumed");

    expect(
        resolveOpenSwipeStart(SwipeDirection::Vertical, false, false, false, -1.0f)
            == OpenSwipeStartAction::ShowOverview,
        "vertical negative swipes should open inactive overviews"
    );
    expect(
        resolveOpenSwipeStart(SwipeDirection::Vertical, false, true, false, 1.0f)
            == OpenSwipeStartAction::HideOverview,
        "vertical positive swipes should close active overviews"
    );
    expect(
        resolveOpenSwipeStart(SwipeDirection::Horizontal, false, false, false, -1.0f)
            == OpenSwipeStartAction::None,
        "horizontal swipes should not trigger open gestures"
    );
    expect(
        resolveOpenSwipeStart(SwipeDirection::Vertical, true, true, false, 1.0f)
            == OpenSwipeStartAction::None,
        "closing views should reject new open gestures"
    );
    expect(
        resolveOpenSwipeStart(SwipeDirection::Vertical, false, false, true, -1.0f)
            == OpenSwipeStartAction::None,
        "navigating views should reject new open gestures"
    );
    expect(
        shouldStartMoveSwipe(false, false, false),
        "inactive stable views should allow move swipes"
    );
    expect(!shouldStartMoveSwipe(true, false, false), "active views should reject move swipes");
    expect(!shouldStartMoveSwipe(false, true, false), "closing views should reject move swipes");
    expect(!shouldStartMoveSwipe(false, false, true), "navigating views should reject move swipes");

    {
        const auto result = nextSwipeAmount(10.0f, -2.0f, 100.0f);
        expect(result.has_value() && *result == 8.0f, "swipe amount should accumulate deltas");
    }
    {
        const auto result = nextSwipeAmount(10.0f, -2.0f, 0.0f);
        expect(!result.has_value(), "swipe amount should reject invalid limits");
    }
    {
        const auto result = openSwipeProgress(50.0f, 100.0f);
        expect(
            result.has_value() && *result == 0.5f,
            "open swipe progress should map distance to progress"
        );
    }
    {
        const auto result = openSwipeProgress(50.0f, 0.0f);
        expect(!result.has_value(), "open swipe progress should reject invalid distances");
    }
    {
        const auto result = shouldKeepOverviewOpen(20.0f, 100.0f);
        expect(result.has_value() && *result, "low swipe progress should keep the overview open");
    }
    {
        const auto result = shouldKeepOverviewOpen(80.0f, 100.0f);
        expect(result.has_value() && !*result, "high swipe progress should close the overview");
    }

    return 0;
}
