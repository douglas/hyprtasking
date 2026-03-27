#include "render_snapshot.hpp"

#include <hyprland/src/managers/input/InputManager.hpp>

#include "globals.hpp"
#include "overview.hpp"
#include "render.hpp"

std::optional<HTRenderSnapshot> capture_render_snapshot(VIEWID view_id, float drag_window_scale) {
    if (ht_manager == nullptr)
        return std::nullopt;

    const PHTVIEW view = ht_manager->get_view_from_id(view_id);
    if (view == nullptr)
        return std::nullopt;

    const PHLMONITOR monitor = view->get_monitor();
    if (monitor == nullptr)
        return std::nullopt;

    HTRenderSnapshot snapshot;
    snapshot.monitor = monitor;
    snapshot.time    = Time::steadyNow();

    const PHTVIEW cursor_view = ht_manager->get_view_from_cursor();
    if (cursor_view == nullptr)
        return snapshot;

    const PHLWINDOW dragged_window = ht_manager->get_dragged_window();
    if (dragged_window == nullptr)
        return snapshot;

    const Vector2D mouse_coords = g_pInputManager->getMouseCoordsInternal();
    const CBox window_box = dragged_window->getWindowMainSurfaceBox()
                                .translate(-mouse_coords)
                                .scale(drag_window_scale)
                                .translate(mouse_coords);
    if (window_box.intersection(monitor->logicalBox()).empty())
        return snapshot;

    snapshot.dragged_window = HTDraggedWindowSnapshot {
        .window = dragged_window,
        .box    = window_box,
    };

    return snapshot;
}

void render_dragged_window_snapshot(const HTRenderSnapshot& snapshot) {
    if (!snapshot.dragged_window.has_value() || snapshot.monitor == nullptr)
        return;

    render_window_at_box(
        snapshot.dragged_window->window,
        snapshot.monitor,
        snapshot.time,
        snapshot.dragged_window->box
    );
}
