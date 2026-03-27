#pragma once

#include <optional>

namespace HTLogic {

enum class SwipeDirection {
    None,
    Horizontal,
    Vertical,
};

enum class OpenSwipeStartAction {
    None,
    ShowOverview,
    HideOverview,
};

SwipeDirection detectSwipeDirection(double delta_x, double delta_y);
float normalizedOpenDelta(float delta_y, bool open_positive);
bool shouldConsumeOpenSwipe(bool active, bool already_open_swipe);
bool shouldConsumeMoveSwipe(bool already_move_swipe);
OpenSwipeStartAction
resolveOpenSwipeStart(SwipeDirection direction, bool closing, bool active, bool navigating, float delta_y);
bool shouldStartMoveSwipe(bool active, bool closing, bool navigating);
std::optional<float> nextSwipeAmount(float swipe_amount, float delta, float limit);
std::optional<float> openSwipeProgress(float swipe_amount, float open_distance);
std::optional<bool> shouldKeepOverviewOpen(float swipe_amount, float open_distance);

} // namespace HTLogic
