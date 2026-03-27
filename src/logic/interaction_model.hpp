#pragma once

namespace HTLogic {

enum class DragStartAction {
    Ignore,
    HideViews,
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

DragStartAction decideDragStart(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool has_workspace_target
);

DragEndAction decideDragEnd(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool has_target,
    bool has_dragged_window,
    bool move_mode
);

SelectStartAction decideSelectStart(
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool has_workspace_target
);

SelectEndAction decideSelectEnd(
    bool select_pending,
    bool has_view,
    bool view_active,
    bool view_closing,
    bool manages_mouse,
    bool has_workspace_target
);

}
