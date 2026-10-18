#pragma once

namespace HTLogic {

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

} // namespace HTLogic
