#pragma once

namespace HTLogic {

enum class DragStartAction {
    Ignore,
    BeginDrag,
};

enum class DragEndAction {
    Ignore,
    FinalizeDrop,
};

enum class SelectStartAction {
    Ignore,
    BeginSelect,
};

enum class SelectEndAction {
    Ignore,
    FinalizeSelect,
    CancelSelect,
};

enum class MouseButtonResult {
    Ignore,
    Consume,
    PassThrough,
};

DragStartAction decideDragStart(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool has_workspace_target
);

DragEndAction decideDragEnd(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool has_target,
    bool has_dragged_window,
    bool move_mode
);

SelectStartAction decideSelectStart(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool has_workspace_target
);

SelectEndAction decideSelectEnd(
    bool select_pending,
    bool has_view,
    bool view_active,
    bool view_closing,
    bool has_workspace_target
);

bool shouldConsumeManagedMouseButton(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool matching_button
);

MouseButtonResult resolveClaimedMouseRelease(
    bool button_claimed,
    bool drag_started,
    bool allow_release_passthrough
);

}
