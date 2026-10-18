#include "gesture_model.hpp"

#include <algorithm>

namespace HTLogic {

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

} // namespace HTLogic
