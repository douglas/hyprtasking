#include "manager.hpp"

#include <algorithm>
#include <format>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include "globals.hpp"
#include "compat/renderer_compat.hpp"
#include "compat/runtime_compat.hpp"
#include "logic/reload_model.hpp"
#include "overview.hpp"
#include "state_guards.hpp"

HTManager::HTManager() {
    reset_swipe_state();
}

namespace {

const char* swipe_state_name(HTManager::swipe_state_t state) {
    switch (state) {
        case HTManager::HT_SWIPE_OPEN:
            return "open";
        case HTManager::HT_SWIPE_MOVE:
            return "move";
        case HTManager::HT_SWIPE_NONE:
            return "none";
    }

    return "unknown";
}

}

PHTVIEW HTManager::get_view_from_monitor(PHLMONITOR monitor) {
    if (monitor == nullptr)
        return nullptr;
    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        if (view->get_monitor() != monitor)
            continue;
        return view;
    }
    return nullptr;
}

PHTVIEW HTManager::get_view_from_cursor() {
    return get_view_from_monitor(HTCompat::cursor_monitor());
}

PHTVIEW HTManager::get_view_from_id(VIEWID view_id) {
    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        if (view->monitor_id != view_id)
            continue;
        return view;
    }
    return nullptr;
}

HTCursorWorkspaceContext HTManager::resolve_cursor_workspace(bool create_if_missing) {
    HTCursorWorkspaceContext context;
    context.monitor = HTCompat::cursor_monitor();
    if (context.monitor == nullptr)
        return context;

    context.view = get_view_from_monitor(context.monitor);
    context.mouse_coords = HTCompat::mouse_coords();
    if (context.view == nullptr)
        return context;

    context.workspace_id = context.view->layout->get_ws_id_from_global(context.mouse_coords);
    context.workspace = HTCompat::resolve_workspace_target(
        context.monitor,
        context.workspace_id,
        create_if_missing
    );
    return context;
}

PHLWINDOW HTManager::get_window_from_cursor(bool return_focused) {
    const HTCursorWorkspaceContext cursor_context = resolve_cursor_workspace(false);
    const PHLMONITOR cursor_monitor = cursor_context.monitor;
    if (cursor_monitor == nullptr)
        return nullptr;

    if (return_focused) {
        const PHLWORKSPACE active_workspace = HTCompat::active_monitor_workspace(cursor_monitor);
        if (active_workspace == nullptr)
            return nullptr;
        return active_workspace->getLastFocusedWindow();
    }

    const PHTVIEW cursor_view = cursor_context.view;
    if (cursor_view == nullptr)
        return nullptr;
    const Vector2D mouse_coords = cursor_context.mouse_coords;

    if (!cursor_view->active || !cursor_view->layout->should_manage_mouse()) {
        return HTCompat::hover_target_window_at(mouse_coords);
    }
    const WORKSPACEID ws_id = cursor_context.workspace_id;
    const PHLWORKSPACE hovered_workspace = cursor_context.workspace;
    if (hovered_workspace == nullptr)
        return nullptr;

    const auto ws_coords =
        cursor_view->layout->global_to_workspace_monitor_coords(mouse_coords, ws_id);
    if (!ws_coords.has_value())
        return nullptr;

    HTScopedMonitorWorkspace restore_workspace(cursor_monitor, true);
    if (!HTCompat::activate_monitor_workspace_internal(cursor_monitor, hovered_workspace))
        return nullptr;
    HTScopedWorkspaceVisibility visible_workspace(hovered_workspace, true);

    const PHLWINDOW hovered_window = HTCompat::hover_target_window_at(*ws_coords);

    return hovered_window;
}

void HTManager::show_all_views() {
    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        view->show();
    }
}

void HTManager::hide_all_views() {
    for (PHTVIEW view : views) {
        if (view == nullptr)
            continue;
        view->hide(false);
    }
}

void HTManager::show_cursor_view() {
    const PHTVIEW view = get_view_from_cursor();
    if (view != nullptr)
        view->show();
}

bool HTManager::has_runtime_view() {
    return std::ranges::any_of(views, [](const PHTVIEW& view) {
        return view != nullptr && view->has_runtime_activity();
    });
}

void HTManager::refresh_cursor_override() {
    HTCompat::set_cursor_override_enabled(has_runtime_view());
}

PHLWINDOW HTManager::get_dragged_window() {
    return dragged_window.lock();
}

bool HTManager::is_dragged_window(PHLWINDOW window) {
    if (window == nullptr)
        return false;

    return get_dragged_window() == window;
}

void HTManager::set_dragged_window(PHLWINDOW window) {
    dragged_window = window;
}

void HTManager::clear_dragged_window() {
    dragged_window = {};
}

void HTManager::reset() {
    runtime_disabled = false;
    disabled_reason.clear();
    HTCompat::reset_overview_render_guard();
    reset_selection_state();
    reset_mouse_button_state();
    clear_dragged_window();
    reset_drag_state();
    reset_swipe_state();
    views.clear();
    refresh_cursor_override();
}

bool HTManager::runtime_enabled() const {
    return !runtime_disabled;
}

void HTManager::disable_runtime(std::string_view source, std::string_view reason) {
    if (runtime_disabled)
        return;

    runtime_disabled = true;
    disabled_reason = std::string(reason);
    const std::string state_snapshot = std::format(
        "views={}, active_view={}, cursor_view={}, selection_pending={}, swipe_state={}, swipe_amt={}, dragged_window={}",
        views.size(),
        has_active_view(),
        cursor_view_active(),
        selection_pending,
        swipe_state_name(swipe_state),
        swipe_amt,
        get_dragged_window() != nullptr
    );
    HTCompat::reset_overview_render_guard();
    reset_selection_state();

    for (const PHTVIEW& view : views) {
        if (view == nullptr)
            continue;
        view->cancel_runtime_state();
    }

    reset_drag_state();
    reset_swipe_state();
    refresh_cursor_override();

    Log::logger->log(
        Log::ERR,
        "[Hyprtasking] runtime disabled after {}: {} ({})",
        source,
        reason,
        state_snapshot
    );
    HyprlandAPI::addNotification(
        PHANDLE,
        "[Hyprtasking] Disabled for this session after an internal runtime failure.",
        CHyprColor {1.0, 0.2, 0.2, 1.0},
        5000
    );
}

std::string HTManager::runtime_disable_reason() const {
    return disabled_reason;
}

void HTManager::reset_selection_state() {
    selection_pending = false;
    pending_mouse_selection_workspace_id = WORKSPACE_INVALID;
    if (claimed_mouse_interaction == HT_MOUSE_SELECT)
        reset_mouse_button_state();
}

PHTVIEW HTManager::get_swipe_view() {
    if (swipe_view_id == INVALID_VIEW_ID)
        return nullptr;

    return get_view_from_id(swipe_view_id);
}

void HTManager::reset_swipe_state() {
    if (const PHTVIEW swipe_view = get_swipe_view(); swipe_view != nullptr)
        swipe_view->set_runtime_state(swipe_view->active, swipe_view->closing, false);

    swipe_state = HT_SWIPE_NONE;
    swipe_amt = 0.0f;
    swipe_view_id = INVALID_VIEW_ID;
    refresh_cursor_override();
}

void HTManager::reset_drag_state() {
    drag_interaction_started = false;
    clear_dragged_window();

    const auto target = HTCompat::drag_controller_target();
    if (const PHLWINDOW dragged_window = target == nullptr ? nullptr : target->window();
        dragged_window != nullptr) {
        HTCompat::reset_window_workspace_move_alpha(dragged_window);
    }

    if (HTCompat::drag_controller_mode() != MBIND_INVALID)
        HTCompat::end_drag_controller();

    HTCompat::set_mouse_bind_mode(MBIND_INVALID);
    if (claimed_mouse_interaction == HT_MOUSE_DRAG)
        reset_mouse_button_state();
}

void HTManager::sync_monitor_views() {
    std::vector<HTLogic::MonitorID> view_ids;
    view_ids.reserve(views.size());
    for (const PHTVIEW& view : views) {
        if (view == nullptr)
            continue;
        view_ids.push_back(view->monitor_id);
    }

    std::vector<HTLogic::MonitorID> monitor_ids;
    const std::vector<PHLMONITOR> monitors = HTCompat::compositor_monitors();
    monitor_ids.reserve(monitors.size());
    for (const PHLMONITOR& monitor : monitors) {
        monitor_ids.push_back(HTCompat::monitor_id(monitor));
    }

    const auto stale_ids = HTLogic::staleMonitorViewIDs(view_ids, monitor_ids);
    bool removed_runtime_view = false;
    std::erase_if(views, [&](const PHTVIEW& view) {
        if (view == nullptr)
            return true;

        if (std::ranges::find(stale_ids, view->monitor_id) == stale_ids.end())
            return false;

        removed_runtime_view = removed_runtime_view || view->has_runtime_activity();
        view->cancel_runtime_state();
        Log::logger->log(
            LOG,
            "[Hyprtasking] Removing stale view for detached monitor id {}",
            view->monitor_id
        );
        return true;
    });

    const bool removed_swipe_view =
        swipe_view_id != INVALID_VIEW_ID && std::ranges::find(stale_ids, swipe_view_id) != stale_ids.end();
    if (removed_runtime_view || removed_swipe_view) {
        reset_selection_state();
        reset_drag_state();
        reset_swipe_state();
        refresh_cursor_override();
    }

    const auto missing_ids = HTLogic::missingMonitorViewIDs(view_ids, monitor_ids);
    for (const auto missing_id : missing_ids) {
        const PHLMONITOR monitor = HTCompat::monitor_from_id(missing_id);
        if (monitor == nullptr)
            continue;

        const Vector2D monitor_size = HTCompat::monitor_transformed_size(monitor);
        views.push_back(makeShared<HTView>(HTCompat::monitor_id(monitor)));
        Log::logger->log(
            LOG,
            "[Hyprtasking] Registering view for monitor {} with resolution {}x{}",
            HTCompat::monitor_description(monitor),
            monitor_size.x,
            monitor_size.y
        );
    }

    for (const PHLMONITOR& monitor : monitors) {
        const PHTVIEW view = get_view_from_monitor(monitor);
        if (view == nullptr || view->active)
            continue;
        view->layout->init_position();
    }
}

bool HTManager::has_active_view() {
    if (!runtime_enabled())
        return false;

    for (const auto& view : views) {
        if (view == nullptr)
            continue;
        if (view->active)
            return true;
    }
    return false;
}

bool HTManager::cursor_view_active() {
    if (!runtime_enabled())
        return false;

    const PHTVIEW view = get_view_from_cursor();
    if (view == nullptr)
        return false;
    return view->active;
}
