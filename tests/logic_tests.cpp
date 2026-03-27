#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

#include "../src/logic/compat_model.hpp"
#include "../src/logic/controller_state.hpp"
#include "../src/logic/dispatch_args.hpp"
#include "../src/logic/gesture_model.hpp"
#include "../src/logic/geometry_model.hpp"
#include "../src/logic/interaction_model.hpp"
#include "../src/logic/layout_model.hpp"
#include "../src/logic/reload_model.hpp"

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

    {
        const auto result = decideCompatSupport(true, "0.54.2", "0.54.");
        expect(result.supported, "compat support should accept supported build versions");
    }
    {
        const auto result = decideCompatSupport(false, "0.54.2", "0.54.");
        expect(!result.supported, "compat support should reject mismatched hashes");
    }
    {
        const auto result = decideCompatSupport(true, "0.55.0", "0.54.");
        expect(!result.supported, "compat support should reject unsupported Hyprland minors");
    }
    expect(
        versionMatchesMinor("v0.54.2", "0.54."),
        "compat version matching should accept v-prefixed supported versions"
    );
    expect(
        !versionMatchesMinor("0.55.0", "0.54."),
        "compat version matching should reject unsupported minors"
    );

    {
        const auto result = parseOffsetArg("12", 3);
        expect(result.ok && result.value == 12, "absolute offset should parse");
    }
    {
        const auto result = parseOffsetArg(" +4 ", 3);
        expect(result.ok && result.value == 7, "relative offset should parse");
    }
    {
        const auto result = parseOffsetArg("", 3);
        expect(!result.ok && result.error == "missing arg", "missing offset arg should fail");
    }
    {
        const auto result = parseOffsetArg("-5", 3);
        expect(
            !result.ok && result.error == "offset cannot be negative",
            "negative resulting offset should fail"
        );
    }
    {
        const auto result = parseOffsetArg("999999999999999999", 3);
        expect(!result.ok, "overflowing offset should fail");
    }

    {
        const auto result = parseLayerOffsetArg("", 0, 2, 3, 4);
        expect(
            result.ok && result.requested_offset == 6 && result.max_offset == 18,
            "empty layer arg should default to +1 layer"
        );
    }
    {
        const auto result = parseLayerOffsetArg("+2", 0, 2, 3, 4);
        expect(result.ok && result.requested_offset == 12, "relative layer arg should parse");
    }
    {
        const auto result = parseLayerOffsetArg("2", 6, 2, 3, 4);
        expect(result.ok && result.requested_offset == 12, "absolute layer arg should parse");
    }
    {
        const auto result = parseLayerOffsetArg("abc", 0, 2, 3, 4);
        expect(!result.ok && result.error == "invalid numeric arg", "invalid layer arg should fail");
    }
    {
        const auto result = parseLayerOffsetArg("", 0, 0, 3, 4);
        expect(!result.ok && result.error == "invalid grid dimensions", "invalid grid dimensions should fail");
    }

    expect(nextLinearDummyWorkspaceID({}, 0) == 1, "empty workspace list should start at 1");
    expect(nextLinearDummyWorkspaceID({}, 15) == 15, "empty workspace list should respect large offset");
    expect(
        nextLinearDummyWorkspaceID({15, 16}, 15) == 17,
        "dummy linear workspace should follow the last existing workspace"
    );
    expect(
        nextLinearDummyWorkspaceID({1, 3, 4}, 0) == 5,
        "dummy linear workspace should skip existing ids"
    );

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

    expect(gridWorkspaceID(0, 0, 2, 3, 2, 1) == 6, "grid workspace ids should match the first view");
    expect(
        gridWorkspaceID(1, 6, 2, 3, 2, 1) == 24,
        "grid workspace ids should match offset views"
    );

    expect(HTLogic::isPositiveFinite(1.0f), "positive finite values should be accepted");
    expect(!HTLogic::isPositiveFinite(0.0f), "zero should be rejected as invalid geometry");
    expect(!HTLogic::isPositiveFinite(-1.0f), "negative values should be rejected as invalid geometry");
    expect(HTLogic::isFinitePoint(1.0f, 2.0f), "finite points should be accepted");
    expect(!HTLogic::isFinitePoint(INFINITY, 2.0f), "non-finite x should be rejected");
    expect(!HTLogic::isFinitePoint(1.0f, -INFINITY), "non-finite y should be rejected");
    {
        const auto result = HTLogic::workspaceWidthScale(200.0f, 100.0f);
        expect(result.has_value() && *result == 2.0f, "workspace width scale should divide valid widths");
    }
    {
        const auto result = HTLogic::workspaceWidthScale(200.0f, 0.0f);
        expect(!result.has_value(), "workspace width scale should reject zero monitor width");
    }
    {
        const auto result = HTLogic::dragWindowScale(1.5f);
        expect(result.has_value() && *result == 1.5f, "drag window scale should accept positive finite values");
    }
    {
        const auto result = HTLogic::dragWindowScale(0.0f);
        expect(!result.has_value(), "drag window scale should reject zero");
    }
    {
        const auto result = HTLogic::inverseDragWindowScale(2.0f);
        expect(result.has_value() && *result == 0.5f, "inverse drag window scale should invert valid scales");
    }
    {
        const auto result = HTLogic::inverseDragWindowScale(-1.0f);
        expect(!result.has_value(), "inverse drag window scale should reject invalid scales");
    }
    {
        const auto result = HTLogic::windowRenderScale(150.0f, 100.0f, 75.0f, 50.0f);
        expect(result.has_value() && *result == 2.0f, "window render scale should divide valid widths");
    }
    {
        const auto result = HTLogic::windowRenderScale(150.0f, 100.0f, 0.0f, 50.0f);
        expect(!result.has_value(), "window render scale should reject zero window width");
    }

    expect(
        resolveExitWorkspaceID(true, 9, 3) == 9,
        "exit should prefer the hovered workspace when requested"
    );
    expect(
        resolveExitWorkspaceID(true, -1, 3) == 3,
        "exit should fall back to the active workspace when hovered is invalid"
    );

    expect(
        resolveMoveSourceWorkspace(false, 5, std::optional<long> {7}) == 5,
        "non-window moves should use the active workspace"
    );
    expect(
        resolveMoveSourceWorkspace(true, 5, std::optional<long> {7}) == 7,
        "window moves should use the hovered window workspace"
    );
    expect(
        resolveMoveSourceWorkspace(true, 5, std::nullopt) == -1,
        "window moves should fail without a hovered workspace"
    );

    expect(shouldMoveHoveredWindow(true, true), "move_window should move hovered windows when present");
    expect(!shouldMoveHoveredWindow(true, false), "move_window should not move null hovered windows");
    expect(!shouldMoveHoveredWindow(false, true), "workspace moves should not move hovered windows");
    expect(shouldFocusMovedWindow(true, true), "move_window should focus moved windows when present");
    expect(!shouldFocusMovedWindow(true, false), "move_window should not focus null hovered windows");
    {
        const auto result = resolveMoveExecution(false, false);
        expect(result.valid, "workspace moves should stay valid without a hovered window");
        expect(!result.move_hovered_window, "workspace moves should not move hovered windows");
        expect(!result.focus_moved_window, "workspace moves should not focus moved windows");
        expect(!result.use_move_window_warp, "workspace moves should use workspace warp config");
    }
    {
        const auto result = resolveMoveExecution(true, true);
        expect(result.valid, "move execution should accept hovered-window moves");
        expect(result.move_hovered_window, "move execution should move hovered windows");
        expect(result.focus_moved_window, "move execution should focus moved windows");
        expect(result.use_move_window_warp, "move execution should use move-window warp config");
    }
    {
        const auto result = resolveMoveExecution(true, false);
        expect(!result.valid, "move execution should reject missing hovered windows");
        expect(!result.move_hovered_window, "invalid move execution should not move hovered windows");
        expect(!result.focus_moved_window, "invalid move execution should not focus moved windows");
        expect(!result.use_move_window_warp, "invalid move execution should not use move-window warp");
    }

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
        expect(!result.create_if_missing, "fallback workspace drops should not create new workspaces");
        expect(result.snap_to_workspace, "fallback workspace drops should snap to the workspace");
    }
    {
        const auto result = resolveDropWorkspace(-1, std::nullopt);
        expect(!result.valid, "drop resolution should reject missing hovered and dragged workspaces");
    }
    {
        const auto result = resolveDropWorkspace(-1, std::optional<long> {42});
        expect(result.valid, "drop resolution should accept invalid-hover fallback with dragged workspace");
        expect(result.workspace_id == 42, "drop resolution fallback should keep the dragged workspace id");
        expect(result.snap_to_workspace, "drop resolution fallback should snap to the dragged workspace");
    }

    expect(
        decideDragStart(true, true, false, false, true) == DragStartAction::HideViews,
        "drag start should hide views when the layout does not manage the mouse"
    );
    expect(
        decideDragStart(true, true, false, true, true) == DragStartAction::BeginDrag,
        "drag start should proceed with a valid workspace target"
    );
    expect(
        decideDragStart(true, true, false, true, false) == DragStartAction::Ignore,
        "drag start should reject non-workspace targets"
    );

    expect(
        decideDragEnd(true, true, false, true, true, true, true) == DragEndAction::FinalizeDrop,
        "drag end should finalize valid move drops"
    );
    expect(
        decideDragEnd(true, true, false, true, true, false, true) == DragEndAction::Ignore,
        "drag end should reject missing dragged windows"
    );
    expect(
        decideDragEnd(true, true, false, true, true, true, false) == DragEndAction::Ignore,
        "drag end should reject non-move drag modes"
    );
    expect(
        decideSelectStart(true, true, false, true, true) == SelectStartAction::BeginSelect,
        "select start should begin for valid active workspace targets"
    );
    expect(
        decideSelectStart(true, true, false, true, false) == SelectStartAction::Ignore,
        "select start should ignore invalid workspace targets"
    );
    expect(
        decideSelectEnd(true, true, true, false, true, true) == SelectEndAction::FinalizeSelect,
        "select end should finalize valid pending selections"
    );
    expect(
        decideSelectEnd(true, true, true, false, true, false) == SelectEndAction::CancelSelect,
        "select end should cancel pending selections released off target"
    );
    expect(
        decideSelectEnd(false, true, true, false, true, true) == SelectEndAction::Ignore,
        "select end should ignore releases without a pending selection"
    );

    {
        const auto result = decideViewReload(false, false, false, false, false);
        expect(result.reinitialize_position, "inactive stable views should reinitialize on reload");
        expect(!result.cancel_runtime_state, "inactive stable views should not cancel runtime state");
    }
    {
        const auto result = decideViewReload(true, false, true, false, false);
        expect(result.cancel_runtime_state, "active views should cancel runtime state on reload");
        expect(result.reinitialize_position, "active stable views should reinitialize after reload");
    }
    {
        const auto result = decideViewReload(false, true, true, false, false);
        expect(result.cancel_runtime_state, "active layout changes should cancel runtime state");
        expect(result.change_layout_now, "active layout changes should apply immediately after cancellation");
    }
    {
        const auto result = decideViewReload(false, true, false, false, false);
        expect(result.change_layout_now, "inactive layout changes should apply immediately");
        expect(!result.cancel_runtime_state, "inactive layout changes should not cancel runtime state");
    }
    {
        const auto result = decideViewReload(true, false, false, true, false);
        expect(result.cancel_runtime_state, "closing views should cancel runtime state on reload");
        expect(result.reinitialize_position, "closing views should reset their position on reload");
    }
    {
        const auto result = decideViewReload(false, false, false, false, true);
        expect(!result.reinitialize_position, "navigating views should not be reinitialized without a forced close");
        expect(!result.cancel_runtime_state, "navigating views should be left alone when reload does not force close");
    }

    {
        const auto result = missingMonitorViewIDs({1, 3}, {1, 2, 3, 4});
        expect(result == std::vector<long>({2, 4}), "missing monitor ids should preserve monitor order");
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
    expect(!shouldConsumeOpenSwipe(false, false), "inactive idle views should not consume open swipes");
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
    expect(shouldStartMoveSwipe(false, false, false), "inactive stable views should allow move swipes");
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
        expect(result.has_value() && *result == 0.5f, "open swipe progress should map distance to progress");
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
