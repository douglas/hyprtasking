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

}
