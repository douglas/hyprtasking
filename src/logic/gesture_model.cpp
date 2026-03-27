#include "gesture_model.hpp"

#include <algorithm>
#include <cmath>

namespace HTLogic {

SwipeDirection detectSwipeDirection(double delta_x, double delta_y) {
    if (std::abs(delta_x) > std::abs(delta_y))
        return SwipeDirection::Horizontal;
    if (std::abs(delta_y) > std::abs(delta_x))
        return SwipeDirection::Vertical;
    return SwipeDirection::None;
}

float normalizedOpenDelta(float delta_y, bool open_positive) {
    return open_positive ? delta_y : -delta_y;
}

bool shouldConsumeOpenSwipe(bool active, bool already_open_swipe) {
    return active || already_open_swipe;
}

bool shouldConsumeMoveSwipe(bool already_move_swipe) {
    return already_move_swipe;
}

OpenSwipeStartAction
resolveOpenSwipeStart(
    SwipeDirection direction,
    bool closing,
    bool active,
    bool navigating,
    float delta_y
) {
    if (direction != SwipeDirection::Vertical || closing || navigating)
        return OpenSwipeStartAction::None;

    if (!active && delta_y <= 0.f)
        return OpenSwipeStartAction::ShowOverview;
    if (active && delta_y > 0.f)
        return OpenSwipeStartAction::HideOverview;

    return OpenSwipeStartAction::None;
}

bool shouldStartMoveSwipe(bool active, bool closing, bool navigating) {
    return !active && !closing && !navigating;
}

std::optional<float> nextSwipeAmount(float swipe_amount, float delta, float limit) {
    if (!std::isfinite(limit) || limit <= 0.f)
        return std::nullopt;

    return swipe_amount + delta;
}

std::optional<float> openSwipeProgress(float swipe_amount, float open_distance) {
    if (!std::isfinite(open_distance) || open_distance <= 0.f)
        return std::nullopt;

    return 1.0f - std::clamp(swipe_amount / open_distance, 0.01f, 1.0f);
}

std::optional<bool> shouldKeepOverviewOpen(float swipe_amount, float open_distance) {
    const auto progress = openSwipeProgress(swipe_amount, open_distance);
    if (!progress.has_value())
        return std::nullopt;

    return *progress >= 0.5f;
}

} // namespace HTLogic
