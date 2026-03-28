#include "interaction_model.hpp"

namespace HTLogic {

DragStartAction decideDragStart(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool has_workspace_target
) {
    if (!has_view || !view_active || view_closing)
        return DragStartAction::Ignore;
    if (!manages_mouse)
        return DragStartAction::HideViews;
    if (!has_workspace_target)
        return DragStartAction::Ignore;

    return DragStartAction::BeginDrag;
}

DragEndAction decideDragEnd(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool has_target,
    bool has_dragged_window,
    bool move_mode
) {
    if (!has_view || !view_active || view_closing)
        return DragEndAction::Ignore;
    if (!manages_mouse || !has_target || !has_dragged_window || !move_mode)
        return DragEndAction::Ignore;

    return DragEndAction::FinalizeDrop;
}

SelectStartAction decideSelectStart(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool has_workspace_target
) {
    if (!has_view || !view_active || view_closing || !manages_mouse || !has_workspace_target)
        return SelectStartAction::Ignore;

    return SelectStartAction::BeginSelect;
}

SelectEndAction decideSelectEnd(
    bool select_pending,
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool has_workspace_target
) {
    if (!select_pending)
        return SelectEndAction::Ignore;
    if (!has_view || !view_active || view_closing || !manages_mouse || !has_workspace_target)
        return SelectEndAction::CancelSelect;

    return SelectEndAction::FinalizeSelect;
}

bool shouldConsumeManagedMouseButton(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool matching_button
) {
    return has_view && view_active && !view_closing && manages_mouse && matching_button;
}

MouseButtonResult resolveClaimedMouseRelease(
    bool button_claimed,
    bool drag_started,
    bool allow_release_passthrough
) {
    if (!button_claimed)
        return MouseButtonResult::Ignore;
    if (drag_started && allow_release_passthrough)
        return MouseButtonResult::PassThrough;

    return MouseButtonResult::Consume;
}

}
